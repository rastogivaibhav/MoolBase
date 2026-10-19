#!/usr/bin/env python3
"""V14 adversarial temporal, source, oracle and label leakage contract."""
import copy,hashlib,json,sys,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
sys.path.insert(0,str(HERE.parent/"v13"))
from neutral_observations import convert,fields_at
from run_source_systems import execute

class NeutralTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root=Path(self.temp.name)
        self.public=root/"events"
        self.oracle=root/"secret"
        self.public.mkdir();self.oracle.mkdir()
        for i in range(6):execute(i,self.public,self.oracle)
        self.path=self.public/"LAB-001"/"events.jsonl"
    def events(self):return [json.loads(x) for x in self.path.read_text().splitlines() if x.strip()]
    def save(self,arr):self.path.write_text("".join(json.dumps(x,sort_keys=True)+"\n" for x in arr))
    def fail(self):
        with self.assertRaises(ValueError):convert(self.public)
    def test_all_cases_and_time_cutoffs(self):
        rows,r=convert(self.public)
        self.assertEqual(len(rows),66)
        self.assertEqual(len(r),12)
        self.assertFalse(any(x["cause_labels_emitted"] for x in r))
        self.assertEqual({x["cutoff"] for x in rows},{"T0","T1"})
        self.assertTrue(all(x["source_sha256"] for x in rows))
    def test_deterministic_hashes(self):
        self.assertEqual(convert(self.public),convert(self.public))
    def test_t2_excluded_from_all_inference(self):
        t=self.events()
        t[-1]["payload"]["injected_fault"]="FAKE_LEAK_FROM_T2"
        self.save(t)
        # Later repair data remain excluded even if modified.
        rows,_=convert(self.public)
        self.assertEqual(len(rows),66)
    def test_duplicate_event(self):
        t=self.events();t.append(copy.deepcopy(t[1]));self.save(t);self.fail()
    def test_oracle_key_in_t1(self):
        t=self.events();t[1]["payload"]["injected_fault"]="pending_receipt";self.save(t);self.fail()
    def test_missing_t1_event(self):
        t=self.events();t.pop(1);self.save(t);self.fail()
    def test_forged_source_authority(self):
        t=self.events();t[1]["provenance_class"]="verified";self.save(t);self.fail()
    def test_negative_numeric_measurement(self):
        t=self.events();t[1]["payload"]["posted"]=-2;self.save(t);self.fail()
    def test_mismatched_case_id(self):
        t=self.events();t[1]["case_id"]="LAB-999";self.save(t);self.fail()
    def test_unexpected_producer(self):
        t=self.events();t[1]["source"]="unauthenticated_mqtt";self.save(t);self.fail()
    def test_repaired_outcome_does_not_enter_t1(self):
        t=self.events();t[1]["stage"]="T2";self.save(t);self.fail()
    def test_false_diagnostic_timestamp(self):
        t=self.events();t[1]["event_time"]="2026-10-19T09:20:00Z";self.save(t);self.fail()

if __name__=="__main__":
    unittest.main(verbosity=2)
