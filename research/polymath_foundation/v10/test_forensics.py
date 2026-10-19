#!/usr/bin/env python3
"""Fail-closed scorer integrity tests; synthetic examples are test doubles only."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from score_forensics import evaluate,STAGES,ARMS,COUNTS

def artificial():
    rows=[]
    for stage in STAGES:
        for arm in ARMS:
            paths={"retrieved_target_fibers":3,"retrieved_fiber_paths":3,
                   "support_role_paths":3,"opposition_role_paths":0,
                   "noise_role_paths":0,"unknown_role_paths":0,
                   "support_eligible_paths":0,"opposition_eligible_paths":0,
                   "hypothetical_paths":3,"nonoperative_paths":0,
                   "critical_evidence_missing_paths":3,
                   "opposition_correlation_groups":0,
                   "opposition_representatives_eligible":0}
            rows.append({
                "experiment":"MOOL-POLYMATH-F10-001",
                "stage":stage,"arm":arm,
                "hypothesis_count":3,"f10_source_contract":"synthetic_unverified_only",
                "source_status":"project_authored_synthetic_unverified",
                "model_world_updated":False,"model_world_node_count":0,
                "independently_verified_facts":0,"outcome_feedback_performed":False,
                "native_dwm_capability_enabled":arm=="G2",
                "native_hypokosh_capability_enabled":arm!="G0",
                "actual_graphene_executed":True,
                "initial_bundle":copy.deepcopy(paths),"final_bundle":copy.deepcopy(paths),
                "input_edge_count":{"baseline":3,"contradiction":4,"revision":6}[stage],
                "persistent_contradiction_edges":{"baseline":0,"contradiction":1,"revision":2}[stage],
                "final_opposition_score":0.0,
                "actual_opposition_executed":arm=="G2",
                "operative_selection":False,"selected_hypothesis":"none",
                "dialectical_challenge":False,"opposition_reopen_requested":False
            })
    labels={"external_adjudication":False,"may_be_presented_to_reasoner":False,
             "labels":[{"stage":s,"preferred_hypothesis":"H-B"} for s in STAGES]}
    return rows,labels

class Contract(unittest.TestCase):
    def setUp(self):self.rows,self.labels=artificial()
    def test_baseline_complete(self):
        r=evaluate(self.rows,self.labels)
        self.assertEqual(r["arms"]["G2"]["actual_dwm_calls"],3)
        self.assertEqual(r["external_truth_accuracy"],"NOT_MEASURABLE")
    def test_missing_one_actual_runtime(self):
        with self.assertRaises(ValueError):evaluate(self.rows[:-1],self.labels)
    def test_unverified_becomes_verified(self):
        self.rows[0]["source_status"]="verified"
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_false_scientific_promotion(self):
        self.rows[0]["independently_verified_facts"]=1
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_designer_labels_not_expert_truth(self):
        self.labels["external_adjudication"]=True
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_unaccounted_raw_edges(self):
        self.rows[0]["persistent_contradiction_edges"]=2
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_path_roles_sum(self):
        self.rows[0]["final_bundle"]["opposition_role_paths"]=2
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_missing_correlation_rep(self):
        self.rows[0]["final_bundle"]["opposition_representatives_eligible"]=1
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)
    def test_native_false_execution(self):
        self.rows[-1]["actual_opposition_executed"]=False
        with self.assertRaises(ValueError):evaluate(self.rows,self.labels)

if __name__=="__main__":
    result=unittest.main(verbosity=2,exit=False)
    if not result.result.wasSuccessful():raise SystemExit(1)
