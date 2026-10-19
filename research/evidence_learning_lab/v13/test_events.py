#!/usr/bin/env python3
"""F13 adversarial checks for the operational-event and withheld-answer boundary."""
import copy,importlib.util,sys,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from events import make,validate,digest,canonical,write
class EventTests(unittest.TestCase):
    def setUp(self):self.events,self.truth=make()
    def test_01_basic_integrity(self):
        r=validate(self.events)
        self.assertEqual(r["event_count"],12)
        self.assertEqual(r["incident_count"],3)
        self.assertEqual(r["native_reasoning"],"NOT_RUN")
    def test_02_regeneration_identical(self):
        self.assertEqual(self.events,make()[0])
        self.assertEqual(self.truth,make()[1])
    def test_03_ground_truth_is_separate(self):
        self.assertEqual(self.truth["inference_access"],"WITHHELD")
        self.assertEqual({x["injected_cause"] for x in self.truth["cases"]},
                         {"receipt_posting_delay","wms_snapshot_staleness",
                          "replenishment_policy_misconfiguration"})
        self.assertFalse(any("injected_cause" in canonical(e).decode() for e in self.events))
    def test_04_duplicate_event(self):
        with self.assertRaisesRegex(ValueError,"DUPLICATE_EVENT_ID"):
            validate(self.events+[copy.deepcopy(self.events[0])])
    def test_05_content_tampering(self):
        e=copy.deepcopy(self.events);e[0]["payload"]["reported_on_hand"]=999
        with self.assertRaisesRegex(ValueError,"CONTENT_INTEGRITY_MISMATCH"):validate(e)
    def test_06_fake_source_authority(self):
        e=copy.deepcopy(self.events);e[0]["origin_authority"]="Verified"
        with self.assertRaisesRegex(ValueError,"FORGED_EVIDENCE_AUTHORITY"):validate(e)
    def test_07_answer_leakage(self):
        e=copy.deepcopy(self.events);e[0]["payload"]["injected_cause"]="receipt_posting_delay"
        with self.assertRaisesRegex(ValueError,"LEAKED_GROUND_TRUTH"):validate(e)
    def test_08_whole_record_answer_leakage(self):
        e=copy.deepcopy(self.events);e[0]["ground_truth"]="H-A"
        with self.assertRaisesRegex(ValueError,"INVALID_EVENT_SCHEMA"):validate(e)
    def test_09_producer_spoof(self):
        e=copy.deepcopy(self.events);e[0]["producer_id"]="fault-controller"
        with self.assertRaisesRegex(ValueError,"UNTRUSTED_PRODUCER_NAME"):validate(e)
    def test_10_missing_event_field(self):
        e=copy.deepcopy(self.events);del e[0]["event_time"]
        with self.assertRaisesRegex(ValueError,"INVALID_EVENT_SCHEMA"):validate(e)
    def test_11_wrong_hash(self):
        e=copy.deepcopy(self.events);e[0]["sha256"]="x"*64
        with self.assertRaisesRegex(ValueError,"CONTENT_INTEGRITY_MISMATCH"):validate(e)
    def test_12_non_monotonic_own_source(self):
        e=copy.deepcopy(self.events)
        # Case 101 ERP at T0 and T1. Move later event behind the first.
        for obj in e:
            if obj["case_id"]=="case-101" and obj["producer_id"]=="erp" and obj["phase"]=="T1":
                obj["event_time"]="2026-10-19T10:01:00Z"
                obj["sha256"]=digest({k:v for k,v in obj.items() if k!="sha256"})
                break
        with self.assertRaisesRegex(ValueError,"NONMONOTONIC_SOURCE_TIME"):validate(e)
    def test_13_invalid_event_type(self):
        e=copy.deepcopy(self.events);e[0]["type"]="assert_cause"
        with self.assertRaisesRegex(ValueError,"INVALID_EVENT_TYPE"):validate(e)
    def test_14_future_evidence_wrongly_released(self):
        e=copy.deepcopy(self.events)
        for obj in e:
            if obj["case_id"]=="case-101" and obj["phase"]=="T1":
                obj["phase"]="T0"
                obj["sha256"]=digest({k:v for k,v in obj.items() if k!="sha256"})
        with self.assertRaisesRegex(ValueError,"MISSING_PHASE"):validate(e)
    def test_15_outputs_byte_reproducible(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            write(a);write(b)
            for filename in ("observations.jsonl","evaluator_only_truth.json","validation.json"):
                self.assertEqual((Path(a)/filename).read_bytes(),(Path(b)/filename).read_bytes())
    def test_16_no_unsupported_truth_markers(self):
        e=copy.deepcopy(self.events);e[0]["payload"]["true_cause"]="anything"
        with self.assertRaisesRegex(ValueError,"LEAKED_GROUND_TRUTH"):validate(e)
    def test_17_source_recorded_before_event(self):
        e=copy.deepcopy(self.events);e[0]["recorded_at"]="2026-10-19T09:59:00Z"
        with self.assertRaisesRegex(ValueError,"RECORDED_BEFORE_EVENT"):validate(e)
    def test_18_case_missing_a_phase(self):
        e=[x for x in self.events if not(x["case_id"]=="case-101" and x["phase"]=="T1")]
        with self.assertRaisesRegex(ValueError,"MISSING_PHASE"):validate(e)
if __name__=="__main__":
    u=unittest.main(verbosity=2,exit=False)
    if not u.result.wasSuccessful():raise SystemExit(1)
