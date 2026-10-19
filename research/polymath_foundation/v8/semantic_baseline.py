#!/usr/bin/env python3
"""F8 B1 nonnative ontology baseline, with exactly authored conversions.

The same ontology data is also stored/retrieved in real C++ MoolBase B2.
"""
import argparse
import csv
import math
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from export_typed import write_tsv
from select_baselines import read
from build_trials import QUERY_FIELDS
from build_semantic import ALIAS_FIELDS

AUDIT_FIELDS=("opaque_id","status","reason","alias_count","source_status",
              "node_receipts")

def normalize(q,aliases):
    records=[]
    domain=None
    seen=set()
    target=aliases.get(q["target"])
    if target is None or target["field_kind"]!="output":
        return None,"UNKNOWN_TARGET",0
    if target["alias_unit"]!=q["target_unit"]:
        return None,"TARGET_UNIT_MISMATCH",0
    if target["source_status"]!="unverified":
        return None,"SOURCE_STATUS_MISMATCH",0
    domain=target["domain"]
    for part in q["inputs"].split(";"):
        try:
            name,rest=part.split("=",1)
            number,remainder=rest.split("@",1)
            unit,kind=remainder.split("#",1)
        except ValueError:
            return None,"MALFORMED_TYPED_INPUT",len(records)+1
        a=aliases.get(name)
        if a is None or a["field_kind"]!="input":
            return None,"UNKNOWN_ALIAS",len(records)+1
        if (a["domain"]!=domain or a["source_status"]!="unverified" or
            a["source_family"]!="original-project-authored-"+domain):
            return None,"CROSS_DOMAIN_OR_PROVENANCE",len(records)+1
        if a["canonical"] in seen:
            return None,"DUPLICATE_CANONICAL_CONCEPT",len(records)+1
        if a["alias_unit"]!=unit:
            return None,"INPUT_UNIT_MISMATCH",len(records)+1
        if a["canonical_type"]!=kind:
            return None,"INPUT_TYPE_MISMATCH",len(records)+1
        seen.add(a["canonical"])
        try:
            if kind=="integer":
                # Alias input is a fractional *quantity in alternative units*
                # even when canonical target is an integral item count.
                input_number=float(number)
                if not math.isfinite(input_number):
                    raise ValueError("nonfinite")
                canonical=input_number*int(a["mul_num"])/int(a["mul_den"])
                if not math.isfinite(canonical) or canonical<-(2**63) or canonical>=2**63:
                    return None,"NUMERIC_OVERFLOW",len(records)+1
                if not canonical.is_integer():
                    return None,"NONINTEGRAL_CANONICAL_VALUE",len(records)+1
                val=str(int(canonical))
            else:
                input_number=float(number)
                canonical=input_number*int(a["mul_num"])/int(a["mul_den"])
                if not math.isfinite(canonical):
                    return None,"NONFINITE_CANONICAL_VALUE",len(records)+1
                val=format(canonical,".17g")
        except (ValueError,OverflowError):
            return None,"INVALID_INPUT_NUMBER",len(records)+1
        records.append(f"{a['canonical']}={val}@{a['canonical_unit']}#{kind}")
    if not records:
        return None,"MISSING_INPUTS",0
    normalized={
        "opaque_id":q["opaque_id"],
        "description":q["description"],
        "target":target["canonical"],
        "target_unit":target["canonical_unit"],
        "inputs":";".join(records)
    }
    return normalized,"NORMALIZED_UNVERIFIED",len(records)+1+1

def resolve(queries,aliases):
    out=[]
    audit=[]
    for q in queries:
        row,reason,count=normalize(q,aliases)
        if row is None:
            # A unique non-registered output enforces abstention in the
            # original F7 router. Never forge a valid numerical response.
            row={"opaque_id":q["opaque_id"],"description":q["description"],
                 "target":"__unresolved__", "target_unit":"__unknown__",
                 "inputs":q["inputs"]}
            status="abstain"
        else:
            status="normalized"
        out.append(row)
        audit.append({"opaque_id":q["opaque_id"],"status":status,
                      "reason":reason,"alias_count":str(count),
                      "source_status":"unverified",
                      "node_receipts":"NONNATIVE_BASELINE"})
    return out,audit

def main():
    p=argparse.ArgumentParser()
    for key in ("queries","alias_manifest","normalized","audit"):
        p.add_argument("--"+key,required=True)
    a=p.parse_args()
    rows=read(a.queries,QUERY_FIELDS)
    registry=read(a.alias_manifest,ALIAS_FIELDS)
    entries={e["alias"]:e for e in registry}
    if len(entries)!=len(registry):
        raise ValueError("duplicate alias ID")
    normalized,audit=resolve(rows,entries)
    write_tsv(a.normalized,QUERY_FIELDS,normalized)
    write_tsv(a.audit,AUDIT_FIELDS,audit)
    print(f"f8_nonnative_semantic_normalization=PASS total={len(rows)} "
          f"normalized={sum(x['status']=='normalized' for x in audit)} "
          f"refused={sum(x['status']=='abstain' for x in audit)}")

if __name__=="__main__":
    main()
