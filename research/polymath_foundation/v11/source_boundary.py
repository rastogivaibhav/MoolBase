#!/usr/bin/env python3
"""F11 fail-closed SOURCE-CONTENT INTEGRITY adapter, *not* science verification.

The source registry is supplied separately by a trusted caller; this reference
does NOT establish who curated that registry. It must not set Observed/Verified.
No GrapheneDB ingestion occurs in this research boundary.
"""
import hashlib,json,re
from dataclasses import dataclass
from datetime import datetime
from typing import Dict, List

HASH_RE=re.compile(r"^[a-f0-9]{64}$")
KNOWN_LICENSES={"project_authored_synthetic","CC-BY-4.0","CC0-1.0"}
ACTIVE_STATES={"active"}
ACCEPTED_KINDS={"project_authored_synthetic","external_record_unverified"}

@dataclass(frozen=True)
class Admission:
    status:str
    reason:str
    source_id:str
    source_hash:str
    canonical_family_id:str
    scientific_truth_verified:bool=False
    source_authorship_verified:bool=False
    observed_edge_authorized:bool=False

    def to_dict(self):return vars(self).copy()

def deny(source_id,reason):
    return Admission("REFUSED",reason,str(source_id or "-"),"-","-")

def validate_registry(entries):
    if not isinstance(entries,list) or not entries:
        raise ValueError("trusted source registry must be non-empty list")
    byid={};bycontent={};byorigin={}
    for x in entries:
        if type(x)!=dict:raise ValueError("invalid registry entry")
        for key in ("source_id","content_sha256","family_id","origin_id",
                    "lifecycle_state","license_status","source_kind"):
            if not isinstance(x.get(key),str) or not x[key].strip():
                raise ValueError("missing registry field "+key)
        sid=x["source_id"]
        if sid in byid:raise ValueError("duplicate source id")
        if not HASH_RE.fullmatch(x["content_sha256"]):
            raise ValueError("content hash must be valid SHA-256")
        if x["license_status"] not in KNOWN_LICENSES:
            raise ValueError("unreviewed source licensing")
        if x["source_kind"] not in ACCEPTED_KINDS:
            raise ValueError("unreviewed source-kind cannot authorize observations")
        if x["lifecycle_state"] not in {"active","revoked","superseded","invalidated"}:
            raise ValueError("invalid lifecycle state")
        # Duplicated bytes or variants derived from the same origin MUST have
        # the same family. This check catches falsely independent registry rows.
        for grouping,key in ((bycontent,x["content_sha256"]),(byorigin,x["origin_id"])):
            if key in grouping and grouping[key]!=x["family_id"]:
                raise ValueError("correlated source records assert independent families")
            grouping[key]=x["family_id"]
        if "external_truth_verified" in x or "admitted_as_observed" in x:
            raise ValueError("registry cannot self-grant truth or observation authority")
        byid[sid]=x
    return byid

def verify_claim(raw:bytes, claim:dict, registry:Dict[str,dict])->Admission:
    """Verify a captured source's bytes and exact quoted assertion span only."""
    sid=claim.get("source_id") if isinstance(claim,dict) else "-"
    if not isinstance(raw,bytes):return deny(sid,"RAW_SOURCE_BYTES_REQUIRED")
    if type(claim)!=dict:return deny(sid,"MALFORMED_CLAIM")
    if sid not in registry:return deny(sid,"UNREGISTERED_SOURCE_ID")
    item=registry[sid]
    if item["lifecycle_state"]!="active":
        return deny(sid,"SOURCE_NOT_ACTIVE")
    if claim.get("asserted_origin") in ("Observed","Discovered","Verified"):
        return deny(sid,"SELF_ASSERTED_OBSERVATION_OR_TRUTH")
    if claim.get("scientific_truth_verified") is not False:
        return deny(sid,"NO_INDEPENDENT_TRUTH_ADJUDICATION")
    if claim.get("external_authorship_verified") is not False:
        return deny(sid,"NO_EXTERNAL_AUTHORSHIP_PROOF")
    asserted_hash=claim.get("source_sha256")
    if not isinstance(asserted_hash,str) or not HASH_RE.fullmatch(asserted_hash):
        return deny(sid,"MALFORMED_CONTENT_DIGEST")
    actual_hash=hashlib.sha256(raw).hexdigest()
    if item["content_sha256"]!=actual_hash or asserted_hash!=actual_hash:
        return deny(sid,"CONTENT_HASH_MISMATCH")
    if claim.get("declared_family_id")!=item["family_id"]:
        return deny(sid,"FAMILY_SPOOF_OR_MISMATCH")
    if claim.get("source_kind")!=item["source_kind"]:
        return deny(sid,"SOURCE_KIND_MISMATCH")
    begin=claim.get("start_byte");end=claim.get("end_byte")
    if type(begin)!=int or type(end)!=int or not(0<=begin<end<=len(raw)):
        return deny(sid,"INVALID_BYTE_RANGE")
    quote=claim.get("quoted_assertion")
    if not isinstance(quote,str) or not quote.strip():
        return deny(sid,"MISSING_ASSERTION_SPAN")
    try:
        full=raw.decode("utf8","strict")
        portion=raw[begin:end].decode("utf8","strict")
    except UnicodeDecodeError:
        return deny(sid,"INVALID_UTF8_OR_SPAN_BOUNDARY")
    if portion!=quote:
        return deny(sid,"ASSERTION_SPAN_MISMATCH")
    observed=claim.get("observed_at")
    if not isinstance(observed,str) or not observed:
        return deny(sid,"MISSING_TIMESTAMP")
    try:
        assert observed.endswith("Z") and "T" in observed
        datetime.fromisoformat(observed.replace("Z","+00:00"))
    except (AssertionError,ValueError):
        return deny(sid,"INVALID_OBSERVATION_TIMESTAMP")
    return Admission("SOURCE_CONTENT_INTEGRITY_CHECKED_UNVERIFIED",
                     "BYTES_AND_ASSERTION_SPAN_MATCH_PINNED_SOURCE",
                     sid,actual_hash,item["family_id"])

def independent_families(admissions:List[Admission]):
    return len({a.canonical_family_id for a in admissions
                if a.status=="SOURCE_CONTENT_INTEGRITY_CHECKED_UNVERIFIED"})

def main():
    import argparse
    from pathlib import Path
    p=argparse.ArgumentParser()
    for field in ("registry","raw-source","claim","out"):
        p.add_argument("--"+field,required=True)
    a=p.parse_args()
    registry=validate_registry(json.loads(Path(a.registry).read_text()))
    claim=json.loads(Path(a.claim).read_text())
    result=verify_claim(Path(a.raw_source).read_bytes(),claim,registry)
    Path(a.out).write_text(json.dumps(result.to_dict(),indent=2,sort_keys=True)+"\n")
    print("f11_source_admission="+result.status+" reason="+result.reason+
          " external_science_verified=FALSE observed_edge_authorized=FALSE")
    if result.status!="SOURCE_CONTENT_INTEGRITY_CHECKED_UNVERIFIED":
        raise SystemExit(2)
if __name__=="__main__":main()
