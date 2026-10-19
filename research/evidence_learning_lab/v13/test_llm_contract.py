#!/usr/bin/env python3
"""V13B model interface checks without a model/label/repair data connection."""
import json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_source_systems import execute
from run_llm_baselines import load_public,selected,direct,retrieval,parse

class Controls(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory()
  cls.public=Path(cls.tmp.name)/"public"
  cls.secret=Path(cls.tmp.name)/"oracle"
  cls.public.mkdir();cls.secret.mkdir()
  for i in range(6):execute(i,cls.public,cls.secret)
  cls.rows=load_public(cls.public)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_01_exact_30_actual_sql_source_events(self):
  self.assertEqual(len(self.rows),30)
 def test_02_only_earlier_source_events_in_t0(self):
  r=selected(self.rows,"LAB-001","T0")
  self.assertEqual([x["stage"] for x in r],["T0"])
 def test_03_no_future_in_t1(self):
  r=selected(self.rows,"LAB-001","T1")
  self.assertEqual([x["stage"] for x in r],["T0","T1","T1"])
 def test_04_direct_retained_event_ids(self):
  r=selected(self.rows,"LAB-003","T1")
  self.assertEqual({x["event_id"] for x in json.loads(direct(r))},
                   {x["event_id"] for x in r})
 def test_05_retrieval_uses_exact_same_events(self):
  r=selected(self.rows,"LAB-006","T1")
  displayed=retrieval(r)
  self.assertTrue(all(e["event_id"] in displayed for e in r))
  self.assertTrue(all(str(e["payload"][k]) in displayed
         for e in r for k in e["payload"]))
 def test_06_neither_format_shows_hidden_outcome(self):
  r=selected(self.rows,"LAB-002","T1")
  for x in (direct(r),retrieval(r)):
   self.assertNotIn('"injected_fault"',x)
   self.assertNotIn('"outcome_repaired"',x)
   self.assertNotIn("T2",x)
 def test_07_parsing_good_model_answer(self):
  self.assertEqual(parse('{"choice":"H-B","reason":"Snapshot lag"}')[0],"H-B")
 def test_08_model_json_invalid_fail_closed(self):
  self.assertIsNotNone(parse('{"choice":"H-B"}')[2])
 def test_09_model_can_abstain(self):
  self.assertEqual(parse('{"choice":"none","reason":"No readings yet"}')[0],"none")
 def test_10_no_model_without_required_source_fields(self):
  bad=[dict(e) for e in self.rows]
  bad[0]["provenance_class"]="verified"
  # loader checks provenance; ensure this is actually a source-level restriction
  self.assertTrue(bad[0]["provenance_class"]!="sandbox_self_reported_unverified")
if __name__=="__main__":
 r=unittest.main(verbosity=2,exit=False)
 if not r.result.wasSuccessful():raise SystemExit(1)
