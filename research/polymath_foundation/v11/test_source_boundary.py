#!/usr/bin/env python3
"""F11 experimental source-integrity unit and adversarial refusal checks."""
import copy,hashlib,unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).parent))
from source_boundary import Admission,verify_claim,validate_registry,independent_families

TEXT="A project-authored instrument reports stock queue=12 units.\n"
RAW=TEXT.encode("utf8")
START=TEXT.index("stock queue")
END=TEXT.index("units.")+len("units")
DIGEST=hashlib.sha256(RAW).hexdigest()
def row(sid="fixture-001",family="same-original-family",origin="fixture-origin"):
    return dict(source_id=sid,content_sha256=DIGEST,family_id=family,
       origin_id=origin,lifecycle_state="active",
       license_status="project_authored_synthetic",
       source_kind="project_authored_synthetic")
def claim(sid="fixture-001"):
    return {"source_id":sid,"source_sha256":DIGEST,
     "declared_family_id":"same-original-family",
     "source_kind":"project_authored_synthetic",
     "asserted_origin":"Hypothetical",
     "scientific_truth_verified":False,
     "external_authorship_verified":False,
     "start_byte":START,"end_byte":END,
     "quoted_assertion":TEXT[START:END],
     "observed_at":"2026-10-19T10:00:00Z"}

class SourceBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.registry=validate_registry([row()])
        self.claim=claim()
    def verify(self,raw=RAW,claim_=None,registry_=None):
        return verify_claim(raw,claim_ if claim_ is not None else self.claim,
                            registry_ if registry_ is not None else self.registry)
    def test_01_project_authored_bytes_admitted_but_not_trusted_as_truth(self):
        r=self.verify()
        self.assertEqual(r.status,"SOURCE_CONTENT_INTEGRITY_CHECKED_UNVERIFIED")
        self.assertFalse(r.scientific_truth_verified or r.observed_edge_authorized or r.source_authorship_verified)
    def test_02_unregistered_id_rejected(self):
        c=claim("unknown");self.assertEqual(self.verify(claim_=c).reason,"UNREGISTERED_SOURCE_ID")
    def test_03_forged_claim_content_hash_rejected(self):
        c=claim();c["source_sha256"]="0"*64
        self.assertEqual(self.verify(claim_=c).reason,"CONTENT_HASH_MISMATCH")
    def test_04_tampered_raw_bytes_rejected(self):
        self.assertEqual(self.verify(raw=RAW+b"attack").reason,"CONTENT_HASH_MISMATCH")
    def test_05_blank_digest_rejected(self):
        c=claim();c["source_sha256"]=""
        self.assertEqual(self.verify(claim_=c).reason,"MALFORMED_CONTENT_DIGEST")
    def test_06_forged_family_claim_rejected(self):
        c=claim();c["declared_family_id"]="independent family A"
        self.assertEqual(self.verify(claim_=c).reason,"FAMILY_SPOOF_OR_MISMATCH")
    def test_07_wrong_assertion_text_rejected(self):
        c=claim();c["quoted_assertion"]="stock queue=100 units"
        self.assertEqual(self.verify(claim_=c).reason,"ASSERTION_SPAN_MISMATCH")
    def test_08_missing_assertion_rejected(self):
        c=claim();c["quoted_assertion"]=""
        self.assertEqual(self.verify(claim_=c).reason,"MISSING_ASSERTION_SPAN")
    def test_09_invalid_range_rejected(self):
        c=claim();c["end_byte"]=len(RAW)+100
        self.assertEqual(self.verify(claim_=c).reason,"INVALID_BYTE_RANGE")
    def test_10_wrong_source_kind_rejected(self):
        c=claim();c["source_kind"]="independently_verified"
        self.assertEqual(self.verify(claim_=c).reason,"SOURCE_KIND_MISMATCH")
    def test_11_fake_observed_label_rejected(self):
        c=claim();c["asserted_origin"]="Observed"
        self.assertEqual(self.verify(claim_=c).reason,"SELF_ASSERTED_OBSERVATION_OR_TRUTH")
    def test_12_fake_discovered_label_rejected(self):
        c=claim();c["asserted_origin"]="Discovered"
        self.assertEqual(self.verify(claim_=c).reason,"SELF_ASSERTED_OBSERVATION_OR_TRUTH")
    def test_13_claimed_scientific_truth_verified_rejected(self):
        c=claim();c["scientific_truth_verified"]=True
        self.assertEqual(self.verify(claim_=c).reason,"NO_INDEPENDENT_TRUTH_ADJUDICATION")
    def test_14_claimed_external_authorship_rejected(self):
        c=claim();c["external_authorship_verified"]=True
        self.assertEqual(self.verify(claim_=c).reason,"NO_EXTERNAL_AUTHORSHIP_PROOF")
    def test_15_revoked_source_rejected(self):
        registry=validate_registry([dict(row(),lifecycle_state="revoked")])
        self.assertEqual(self.verify(registry_=registry).reason,"SOURCE_NOT_ACTIVE")
    def test_16_superseded_source_rejected(self):
        registry=validate_registry([dict(row(),lifecycle_state="superseded")])
        self.assertEqual(self.verify(registry_=registry).reason,"SOURCE_NOT_ACTIVE")
    def test_17_bad_timestamp_rejected(self):
        c=claim();c["observed_at"]="tomorrow"
        self.assertEqual(self.verify(claim_=c).reason,"INVALID_OBSERVATION_TIMESTAMP")
    def test_18_missing_timestamp_rejected(self):
        c=claim();c["observed_at"]=""
        self.assertEqual(self.verify(claim_=c).reason,"MISSING_TIMESTAMP")
    def test_19_duplicate_source_identity_rejected(self):
        with self.assertRaises(ValueError):
            validate_registry([row(),row()])
    def test_20_duplicate_content_cannot_fake_independent_family(self):
        with self.assertRaises(ValueError):
            validate_registry([row(),row("mirror","forged-independent","different")])
    def test_21_same_origin_cannot_fake_independent_family(self):
        other=dict(row("mirror",family="other",origin="fixture-origin"),
                   content_sha256="1"*64)
        with self.assertRaises(ValueError):
            validate_registry([row(),other])
    def test_22_duplicate_lineage_counts_once(self):
        other=dict(row("mirror",origin="fixture-origin"))
        r=validate_registry([row(),other])
        c=claim("mirror")
        self.assertEqual(independent_families([self.verify(),
               self.verify(claim_=c,registry_=r)]),1)
    def test_23_registry_cannot_grant_verified_truth(self):
        with self.assertRaises(ValueError):
            validate_registry([dict(row(),external_truth_verified=True)])
    def test_24_registry_cannot_grant_observed_claims(self):
        with self.assertRaises(ValueError):
            validate_registry([dict(row(),admitted_as_observed=True)])
    def test_25_unlicensed_source_rejected(self):
        with self.assertRaises(ValueError):
            validate_registry([dict(row(),license_status="unlicensed")])
    def test_26_new_utf8_boundaries_check(self):
        raw="é".encode("utf8")
        c=claim();c["source_sha256"]=hashlib.sha256(raw).hexdigest()
        c["start_byte"]=0;c["end_byte"]=1;c["quoted_assertion"]="é"
        reg=validate_registry([dict(row(),content_sha256=c["source_sha256"])])
        self.assertEqual(self.verify(raw=raw,claim_=c,registry_=reg).reason,
                         "INVALID_UTF8_OR_SPAN_BOUNDARY")
    def test_27_non_bytes_are_rejected(self):
        self.assertEqual(self.verify(raw=TEXT).reason,"RAW_SOURCE_BYTES_REQUIRED")

if __name__=="__main__":
    result=unittest.main(verbosity=2,exit=False)
    if not result.result.wasSuccessful():raise SystemExit(1)
