#!/usr/bin/env python3
import unittest

import evaluate_v1


TASK = {
    "id": "EP_METRIC",
    "terminal_supported": "H2",
    "minimum_independent_families_for_resolution": 2,
    "events": [
        {
            "step": 1,
            "id": "e1",
            "family": "F_A",
            "kind": "support",
            "bears_on": "H1",
            "independent": True,
            "decisive": False,
        },
        {
            "step": 2,
            "id": "e2",
            "family": "F_B",
            "kind": "support",
            "bears_on": "H2",
            "independent": True,
            "decisive": False,
        },
        {
            "step": 3,
            "id": "e3",
            "family": "F_C",
            "kind": "refute",
            "bears_on": "H1",
            "independent": True,
            "decisive": True,
        },
        {
            "step": 4,
            "id": "e4",
            "family": "F_D",
            "kind": "support",
            "bears_on": "H2",
            "depends_on": ["e2"],
            "independent": True,
            "decisive": False,
        },
        {
            "step": 4,
            "id": "e5",
            "family": "F_D",
            "kind": "support",
            "bears_on": "H2",
            "depends_on": ["e2"],
            "independent": False,
            "decisive": False,
        },
    ],
}


def full_receipt(include_duplicate=False):
    final_refs = ["e2", "e4"] + (["e5"] if include_duplicate else [])
    return {
        "episode_id": "EP_METRIC",
        "configuration": "G2",
        "decisions": [
            {
                "step": 1,
                "status": "provisional",
                "hypothesis": "H1",
                "evidence_refs": ["e1"],
            },
            {
                "step": 3,
                "status": "open",
                "hypothesis": "H1",
                "evidence_refs": ["e1", "e3"],
            },
            {
                "step": 4,
                "status": "resolved",
                "hypothesis": "H2",
                "evidence_refs": final_refs,
            },
        ],
        "terminal_status": "resolved",
        "terminal_hypothesis": "H2",
        "evidence_refs": ["e1", "e2", "e3", "e4"] + (
            ["e5"] if include_duplicate else []
        ),
        "hypotheses": [
            {"id": "H1", "status": "downgraded"},
            {"id": "H2", "status": "resolved"},
        ],
        "transitions": [
            {
                "step": 3,
                "type": "reopen",
                "from": "H1",
                "to": "H1",
                "evidence_refs": ["e3"],
            },
            {
                "step": 4,
                "type": "revise",
                "from": "H1",
                "to": "H2",
                "evidence_refs": final_refs,
            },
        ],
        "challenges": [
            {
                "step": 3,
                "hypothesis": "H1",
                "competing_hypotheses": ["H1", "H2"],
                "reopen_nodes": ["H1", "H2"],
                "evidence_refs": ["e3"],
            }
        ],
        "evidence_metadata": {
            "e1": {
                "family": "F_A",
                "kind": "support",
                "bears_on": "H1",
                "depends_on": [],
            },
            "e2": {
                "family": "F_B",
                "kind": "support",
                "bears_on": "H2",
                "depends_on": [],
            },
            "e3": {
                "family": "F_C",
                "kind": "refute",
                "bears_on": "H1",
                "depends_on": [],
            },
            "e4": {
                "family": "F_D",
                "kind": "support",
                "bears_on": "H2",
                "depends_on": ["e2"],
            },
            "e5": {
                "family": "F_D",
                "kind": "support",
                "bears_on": "H2",
                "depends_on": ["e2"],
            },
        },
        "execution_receipts": [
            {
                "graphene_executed": True,
                "hypokosh_capability_enabled": True,
                "dwm_capability_enabled": True,
                "opposition_research_enabled": True,
                "path_verifier_executed": False,
                "stability_critic_executed": True,
                "epistemic_admissibility_executed": True,
                "convergence_executed": True,
                "opposition_executed": True,
                "bounded_recovery_executed": True,
                "governed_projection_executed": True,
                "model_world_updated": False,
                "terminal_cause": "earned_resolution",
            }
        ],
        "adapter_failure": False,
        "adapter_failure_detail": [],
        "adapter_receipt": {
            "adapter": "G2",
            "runtime_events_observed": 8,
            "synthetic_semantic_events": 0,
            "notes": [],
        },
    }


class EvaluatorMetricTests(unittest.TestCase):
    def test_all_preregistered_metrics_have_explicit_counts(self):
        row = evaluate_v1.score_episode(TASK, full_receipt())
        self.assertFalse(row["excluded_from_primary_metrics"])
        self.assertEqual(row["evidence_use"]["numerator"], 4)
        self.assertEqual(row["evidence_use"]["denominator"], 4)
        self.assertEqual(row["evidence_use"]["rate"], 1.0)
        self.assertTrue(row["refutation_response"]["responded"])
        self.assertEqual(row["revision_inertia_steps"], 0)
        self.assertFalse(row["false_convergence"]["false_convergence"])
        independent = row["independent_evidence_convergence"]
        self.assertEqual(independent["terminal_raw_supporting_items"], 2)
        self.assertEqual(independent["terminal_independent_family_count"], 2)
        self.assertEqual(
            independent["terminal_correlated_or_nonindependent_items"], 0
        )
        self.assertTrue(independent["earned_resolution"])
        coverage = row["selective_coverage"]
        self.assertTrue(coverage["covered"])
        self.assertTrue(coverage["terminal_correct"])
        completeness = row["receipt_completeness"]
        self.assertTrue(completeness["complete"])
        self.assertEqual(completeness["completeness_rate"], 1.0)

    def test_same_family_duplicate_does_not_increase_independent_count(self):
        row = evaluate_v1.score_episode(TASK, full_receipt(include_duplicate=True))
        independent = row["independent_evidence_convergence"]
        self.assertEqual(independent["terminal_raw_supporting_items"], 3)
        self.assertEqual(independent["terminal_independent_family_count"], 2)
        self.assertEqual(
            independent["terminal_correlated_or_nonindependent_items"], 1
        )

    def test_receipt_completeness_exposes_missing_provenance(self):
        receipt = full_receipt()
        receipt["evidence_metadata"] = {}
        row = evaluate_v1.score_episode(TASK, receipt)
        completeness = row["receipt_completeness"]
        self.assertFalse(completeness["complete"])
        self.assertIn(
            "evidence_family_identity", completeness["missing_fields"]
        )
        self.assertIn("dependency_identity", completeness["missing_fields"])
        self.assertNotIn("refutation_evidence", completeness["missing_fields"])

    def test_receipt_completeness_fails_when_visible_revision_lacks_transition(self):
        receipt = full_receipt()
        receipt["transitions"] = [
            transition
            for transition in receipt["transitions"]
            if transition["type"] != "revise"
        ]
        row = evaluate_v1.score_episode(TASK, receipt)
        completeness = row["receipt_completeness"]
        self.assertFalse(completeness["complete"])
        self.assertIn("revision_transition", completeness["missing_fields"])

    def test_revision_inertia_counts_only_later_operative_steps(self):
        receipt = full_receipt()
        receipt["transitions"][0] = {
            "step": 5,
            "type": "reopen",
            "from": "H1",
            "to": "H1",
            "evidence_refs": ["e3"],
        }
        receipt["transitions"][1]["step"] = 5
        receipt["decisions"].insert(
            2,
            {
                "step": 4,
                "status": "open",
                "hypothesis": "H1",
                "evidence_refs": ["e1", "e3"],
            },
        )
        receipt["decisions"][-1]["step"] = 5
        row = evaluate_v1.score_episode(TASK, receipt)
        self.assertEqual(row["revision_inertia_steps"], 1)

    def test_false_convergence_detects_resolution_before_threshold(self):
        receipt = full_receipt()
        receipt["decisions"] = [
            {
                "step": 1,
                "status": "resolved",
                "hypothesis": "H1",
                "evidence_refs": ["e1"],
            }
        ]
        receipt["terminal_status"] = "resolved"
        receipt["terminal_hypothesis"] = "H1"
        row = evaluate_v1.score_episode(TASK, receipt)
        self.assertTrue(row["false_convergence"]["false_convergence"])
        self.assertEqual(len(row["false_convergence"]["violations"]), 1)

    def test_abstention_is_reported_separately_from_coverage(self):
        task = dict(TASK)
        task["terminal_supported"] = "ABSTAIN"
        receipt = full_receipt()
        receipt["terminal_status"] = "abstain"
        receipt["terminal_hypothesis"] = None
        receipt["decisions"][-1]["status"] = "abstain"
        receipt["decisions"][-1]["hypothesis"] = None
        row = evaluate_v1.score_episode(task, receipt)
        selective = row["selective_coverage"]
        self.assertTrue(selective["abstained"])
        self.assertFalse(selective["covered"])
        self.assertTrue(selective["terminal_correct"])

    def test_adapter_failure_is_preserved_and_excluded(self):
        failure = full_receipt()
        failure["adapter_failure"] = True
        failure["adapter_failure_detail"] = ["native receipt missing"]
        row = evaluate_v1.score_episode(TASK, failure)
        self.assertTrue(row["excluded_from_primary_metrics"])
        self.assertEqual(row["failure_class"], "adapter_failure")
        aggregate = evaluate_v1.aggregate([row])
        self.assertEqual(aggregate["eligible_episodes"], 0)
        self.assertEqual(aggregate["excluded_episodes"], 1)
        self.assertEqual(
            aggregate["failure_classes"]["adapter_failure"], 1
        )

    def test_aggregate_uses_raw_numerators_and_denominators(self):
        first = evaluate_v1.score_episode(TASK, full_receipt())
        second_receipt = full_receipt()
        second_receipt["decisions"][0]["evidence_refs"] = []
        second = evaluate_v1.score_episode(TASK, second_receipt)
        result = evaluate_v1.aggregate([first, second])
        self.assertEqual(result["evidence_use_rate"]["numerator"], 7)
        self.assertEqual(result["evidence_use_rate"]["denominator"], 8)
        self.assertEqual(result["evidence_use_rate"]["rate"], 7 / 8)


if __name__ == "__main__":
    unittest.main()
