#!/usr/bin/env python3
"""F13 reproducibility and source/ground truth isolation negative controls."""
import copy,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_source_systems import execute
from derive_evidence import transform,read_public,decision
from score_event_lab import evaluate,load_oracles

class ExecutionContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        cls.root=Path(cls.tmp.name)
        cls.public=cls.root/"public"
        cls.hidden=cls.root/"sealed_controller_only"
        cls.public.mkdir();cls.hidden.mkdir()
        cls.executions=[execute(i,cls.public,cls.hidden) for i in range(6)]
        cls.graph,cls.baseline,cls.cutoffs=transform(cls.public)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_01_real_sqlite_events_and_repairs(self):
        self.assertEqual(len(self.executions),6)
        self.assertEqual(len(load_oracles(self.hidden)),6)
        for x in self.executions:
            self.assertTrue(x["sql_transactions_executed"])
    def test_02_source_events_are_not_oracle_records(self):
        for f in self.public.glob("LAB-*/events.jsonl"):
            raw=f.read_text()
            self.assertNotIn('"injected_fault"',raw)
            self.assertNotIn('"outcome_repaired"',raw)
            self.assertNotIn('"independent_external_adjudication"',raw)
    def test_03_fixed_two_cutoff_rule_baseline(self):
        self.assertEqual(len(self.baseline),12)
        self.assertEqual(sum(r["prediction"]=="none" for r in self.baseline),6)
        self.assertEqual(sum(r["prediction"]!="none" for r in self.baseline),6)
    def test_04_ground_truth_only_accessible_to_evaluator(self):
        self.assertFalse(list(self.public.rglob("*.oracle.json")))
        self.assertEqual(len(list(self.hidden.glob("*.oracle.json"))),6)
    def test_05_graph_has_exact_54_derived_edges(self):
        self.assertEqual(len(self.graph),54)
        self.assertTrue(all(len(r["source_sha256"])==64 for r in self.graph))
    def test_06_future_events_cannot_affect_t0_or_t1_results(self):
        original=self.public/"LAB-001"/"events.jsonl"
        content=original.read_text()
        future={"case_id":"LAB-001","stage":"T2","event_id":"future-diagnostic",
         "event_time":"2026-10-19T10:30:00Z","source":"erp_adapter",
         "event_type":"inventory_status",
         "payload":{"posted":0,"pending_receipts":999,"reorder_min":0},
         "provenance_class":"sandbox_self_reported_unverified"}
        try:
            with original.open("a") as file:file.write(json.dumps(future)+"\n")
            new_graph,new_baselines,_=transform(self.public)
            self.assertEqual(self.graph,new_graph)
            self.assertEqual(self.baseline,new_baselines)
        finally:original.write_text(content)
    def test_07_baseline_does_not_require_controller_truth(self):
        one=self.public/"LAB-001"/"events.jsonl"
        ev=read_public(one,"T1")
        before=decision(ev,"T1")
        self.assertEqual(before[0],"H-A")
        self.assertEqual(before[1],"HANDWRITTEN_DIAGNOSTIC_RULE")
    def test_08_wms_snapshot_evidence_comes_from_wms(self):
        rows=[r for r in self.graph if r["case_id"]=="LAB-003" and
              r["cutoff"]=="T1" and r["strength"]>0.7]
        self.assertEqual(len(rows),3)
        self.assertTrue(all(r["source_system"]=="wms_adapter" for r in rows))
    def test_09_refuse_forged_event_producer(self):
        f=self.public/"LAB-001"/"events.jsonl";old=f.read_text()
        try:
            a=json.loads(old.splitlines()[0]);a["source"]="fake_authenticated_erp"
            f.write_text(json.dumps(a)+"\n"+"\n".join(old.splitlines()[1:])+"\n")
            with self.assertRaises(ValueError):transform(self.public)
        finally:f.write_text(old)
    def test_10_failure_if_one_incident_oracle_missing(self):
        docs=load_oracles(self.hidden)
        self.assertEqual(len(docs),6)
        one=self.hidden/"LAB-001.oracle.json"
        moved=self.hidden/"moved.oracle.off"
        one.rename(moved)
        try:
            with self.assertRaises(ValueError):load_oracles(self.hidden)
        finally:moved.rename(one)
    def test_11_different_variants_for_each_fault_family(self):
        labels=load_oracles(self.hidden)
        self.assertEqual({v["injected_fault"] for v in labels.values()},
                         {"pending_receipt","stale_snapshot","reorder_rule"})
        self.assertEqual(sorted(v["scenario_variant"] for v in labels.values()),[0,0,0,1,1,1])

if __name__=="__main__":
    r=unittest.main(verbosity=2,exit=False)
    if not r.result.wasSuccessful():raise SystemExit(1)
