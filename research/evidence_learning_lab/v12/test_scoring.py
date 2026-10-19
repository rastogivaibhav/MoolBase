#!/usr/bin/env python3
"""V12 evaluation integrity tests; manufactured inputs used ONLY to test scorer."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from score_native_lab import evaluate

def fixtures():
    truth={"experiment":"MOOL-EVIDENCE-LEARNING-V12-001",
           "presented_to_reasoner":False,"external_adjudication":False,
           "incidents":[{"case_id":f"incident-{i+101}","injected_cause":f"H-{chr(65+i)}"}
                        for i in range(3)]}
    rows=[]
    for i in range(3):
        for stage in ("T0","T1"):
            for arm in ("G0","G1","G2"):
                rows.append({"experiment":"MOOL-EVIDENCE-LEARNING-V12-001",
                    "case_id":f"incident-{i+101}","stage":stage,"arm":arm,
                    "reasoning_mode":"Theoretical",
                    "generic_telemetry_quarantined":True,
                    "source_authenticated":False,
                    "scientific_truth_verified":False,
                    "independent_empirical_outcome_observed":False,
                    "model_world_updated":False,
                    "evidence_authority":"unverified_synthetic",
                    "incident_hypotheses":3,
                    "native_graph_edges":4 if stage=="T0" else 6,
                    "has_proposed_answer":arm!="G0",
                    "selected_hypothesis":"none" if arm=="G0" else "H-A",
                    "actual_dwm_opposition":arm=="G2",
                    "status":"speculative","dialectical_challenge":False,
                    "reopen_requested":False,"elapsed_ms":2.25})
    return rows,truth

class EvaluationTests(unittest.TestCase):
    def setUp(self):self.events,self.truth=fixtures()
    def test_complete_native_cube(self):
        r=evaluate(self.events,self.truth)
        self.assertEqual(r["native_calls"],18)
        self.assertEqual(r["empirical_accuracy"],"NOT_MEASURABLE")
    def test_outcomes_not_independent(self):
        self.truth["external_adjudication"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_truth_leakage_flag_rejected(self):
        self.truth["presented_to_reasoner"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_missing_native_call_rejected(self):
        with self.assertRaises(ValueError):evaluate(self.events[:-1],self.truth)
    def test_duplicate_native_call_rejected(self):
        self.events[1]=copy.deepcopy(self.events[0])
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_self_verified_science_rejected(self):
        self.events[0]["scientific_truth_verified"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_authenticity_forgery_rejected(self):
        self.events[0]["source_authenticated"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_observed_mode_rejected(self):
        self.events[0]["reasoning_mode"]="Empirical"
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_incorrect_selected_hypothesis_rejected(self):
        self.events[0]["has_proposed_answer"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_missing_dwm_execution_rejected(self):
        self.events[-1]["actual_dwm_opposition"]=False
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_unexpected_modelworld_mutation_rejected(self):
        self.events[0]["model_world_updated"]=True
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_unmatched_temporal_snapshot_rejected(self):
        self.events[0]["native_graph_edges"]=999
        with self.assertRaises(ValueError):evaluate(self.events,self.truth)
    def test_synthetic_matches_are_not_empirical_truth(self):
        r=evaluate(self.events,self.truth)
        self.assertEqual(r["independently_adjudicated_outcomes"],0)
        self.assertEqual(r["learning_on_unseen_cases"],"NOT_RUN")

if __name__=="__main__":
    res=unittest.main(verbosity=2,exit=False)
    if not res.result.wasSuccessful():raise SystemExit(1)
