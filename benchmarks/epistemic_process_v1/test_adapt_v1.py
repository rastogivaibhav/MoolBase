#!/usr/bin/env python3
import unittest

import adapt_v1
import evaluate_v1


class AdapterContractTests(unittest.TestCase):
    def test_b0_does_not_acquire_hypothesis_or_transition_state(self):
        raw = {
            "episode_id": "EP_TEST",
            "configuration": "B0",
            "steps": [
                {
                    "step": 1,
                    "decision": {
                        "status": "provisional",
                        "hypothesis": "H1",
                        "evidence_refs": ["e1"],
                    },
                },
                {
                    "step": 2,
                    "decision": {
                        "status": "resolved",
                        "hypothesis": "H1",
                        "evidence_refs": ["e1", "e2"],
                    },
                },
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertFalse(receipt["adapter_failure"])
        self.assertEqual(receipt["hypotheses"], [])
        self.assertEqual(receipt["transitions"], [])
        self.assertEqual(
            receipt["adapter_receipt"]["synthetic_semantic_events"], 0
        )

    def test_g1_rejects_dwm_semantic_event(self):
        raw = {
            "episode_id": "EP_TEST",
            "configuration": "G1",
            "hypothesis_nodes": {"H1": 101, "H2": 102},
            "edge_to_evidence_ref": {"201": "e1"},
            "steps": [
                {
                    "step": 1,
                    "native_events": [
                        {
                            "source": "dwm",
                            "type": "reopen",
                            "previous_hypothesis_node": 101,
                            "hypothesis_node": 101,
                            "reopen_nodes": [101, 102],
                            "evidence_edges": [201],
                            "epistemic_state": "reopened",
                        },
                        {
                            "source": "graphene_core",
                            "type": "terminal",
                            "hypothesis_node": 101,
                            "evidence_edges": [201],
                            "epistemic_state": "provisionally_resolved",
                        },
                    ],
                }
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertTrue(receipt["adapter_failure"])
        self.assertIn("G1 observed a DWM semantic event",
                      receipt["adapter_failure_detail"][0])
        self.assertEqual(
            receipt["adapter_receipt"]["synthetic_semantic_events"], 0
        )

    def test_g2_translates_only_native_reopen_and_revision(self):
        raw = {
            "episode_id": "EP_TEST",
            "configuration": "G2",
            "hypothesis_nodes": {"H1": 101, "H2": 102},
            "edge_to_evidence_ref": {
                "201": "e1",
                "202": "e4",
                "203": "e5",
            },
            "steps": [
                {
                    "step": 4,
                    "native_events": [
                        {
                            "source": "hypokosh",
                            "type": "hypothesis_set",
                            "competing_hypotheses": [101, 102],
                            "evidence_edges": [201, 202],
                            "epistemic_state": "competing",
                        },
                        {
                            "source": "dwm",
                            "type": "challenge",
                            "hypothesis_node": 101,
                            "competing_hypotheses": [101, 102],
                            "reopen_nodes": [101, 102],
                            "evidence_edges": [202],
                            "epistemic_state": "challenged",
                        },
                        {
                            "source": "dwm",
                            "type": "reopen",
                            "previous_hypothesis_node": 101,
                            "hypothesis_node": 101,
                            "competing_hypotheses": [101, 102],
                            "reopen_nodes": [101, 102],
                            "evidence_edges": [202],
                            "epistemic_state": "reopened",
                        },
                        {
                            "source": "dwm",
                            "type": "revision",
                            "previous_hypothesis_node": 101,
                            "hypothesis_node": 102,
                            "competing_hypotheses": [101, 102],
                            "evidence_edges": [202, 203],
                            "epistemic_state": "revised",
                        },
                        {
                            "source": "graphene_core",
                            "type": "terminal",
                            "hypothesis_node": 102,
                            "competing_hypotheses": [101, 102],
                            "evidence_edges": [202, 203],
                            "epistemic_state": "resolved",
                        },
                    ],
                }
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertFalse(receipt["adapter_failure"])
        self.assertEqual(receipt["terminal_hypothesis"], "H2")
        self.assertEqual(receipt["terminal_status"], "resolved")
        self.assertEqual(
            [transition["type"] for transition in receipt["transitions"]],
            ["reopen", "revise"],
        )
        self.assertEqual(receipt["transitions"][0]["from"], "H1")
        self.assertEqual(receipt["transitions"][1]["to"], "H2")
        self.assertEqual(
            receipt["adapter_receipt"]["synthetic_semantic_events"], 0
        )

    def test_valid_node_zero_is_not_treated_as_missing_hypothesis(self):
        raw = {
            "episode_id": "EP_ZERO",
            "configuration": "G1",
            "hypothesis_nodes": {"H1": 0, "H2": 1},
            "edge_to_evidence_ref": {"10": "e1"},
            "evidence_metadata": {
                "e1": {
                    "family": "F_A",
                    "kind": "support",
                    "bears_on": "H1",
                    "depends_on": [],
                }
            },
            "runtime_receipts": [{"step": 1, "graphene_executed": True}],
            "steps": [
                {
                    "step": 1,
                    "native_events": [
                        {
                            "source": "hypokosh",
                            "type": "hypothesis_set",
                            "hypothesis_node": 0,
                            "competing_hypotheses": [0, 1],
                            "evidence_edges": [10],
                            "epistemic_state": "selected",
                        },
                        {
                            "source": "graphene_core",
                            "type": "terminal",
                            "hypothesis_node": 0,
                            "competing_hypotheses": [0, 1],
                            "evidence_edges": [10],
                            "epistemic_state": "provisionally_resolved",
                        },
                    ],
                }
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertFalse(receipt["adapter_failure"])
        self.assertEqual(receipt["terminal_hypothesis"], "H1")
        self.assertEqual(receipt["decisions"][0]["hypothesis"], "H1")
        self.assertEqual(
            {item["id"] for item in receipt["hypotheses"]},
            {"H1", "H2"},
        )

    def test_explicit_null_is_missing_but_unknown_integer_fails(self):
        mapping = {0: "H1", 1: "H2"}
        self.assertIsNone(adapt_v1._node_to_hypothesis(None, mapping))
        self.assertEqual(adapt_v1._node_to_hypothesis(0, mapping), "H1")
        with self.assertRaises(adapt_v1.AdapterError):
            adapt_v1._node_to_hypothesis(99, mapping)

    def test_native_challenge_reopen_revision_and_audit_metadata_survive(self):
        raw = {
            "episode_id": "EP_AUDIT",
            "configuration": "G2",
            "hypothesis_nodes": {"H1": 0, "H2": 1},
            "edge_to_evidence_ref": {"20": "e4", "21": "e5"},
            "evidence_metadata": {
                "e4": {
                    "family": "F_C",
                    "kind": "refute",
                    "bears_on": "H1",
                    "depends_on": [],
                },
                "e5": {
                    "family": "F_D",
                    "kind": "support",
                    "bears_on": "H2",
                    "depends_on": ["e3"],
                },
            },
            "runtime_receipts": [
                {
                    "step": 4,
                    "graphene_executed": True,
                    "hypokosh_capability_enabled": True,
                    "dwm_capability_enabled": True,
                    "opposition_executed": True,
                }
            ],
            "steps": [
                {
                    "step": 4,
                    "native_events": [
                        {
                            "source": "dwm",
                            "type": "challenge",
                            "hypothesis_node": 0,
                            "competing_hypotheses": [0, 1],
                            "reopen_nodes": [0, 1],
                            "evidence_edges": [20],
                            "epistemic_state": "challenged",
                        },
                        {
                            "source": "dwm",
                            "type": "reopen",
                            "previous_hypothesis_node": 0,
                            "hypothesis_node": 0,
                            "competing_hypotheses": [0, 1],
                            "reopen_nodes": [0, 1],
                            "evidence_edges": [20],
                            "epistemic_state": "reopened",
                        },
                        {
                            "source": "dwm",
                            "type": "revision",
                            "previous_hypothesis_node": 0,
                            "hypothesis_node": 1,
                            "competing_hypotheses": [0, 1],
                            "evidence_edges": [20, 21],
                            "epistemic_state": "revised",
                        },
                        {
                            "source": "graphene_core",
                            "type": "terminal",
                            "hypothesis_node": 1,
                            "competing_hypotheses": [0, 1],
                            "evidence_edges": [20, 21],
                            "epistemic_state": "resolved",
                        },
                    ],
                }
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertFalse(receipt["adapter_failure"])
        self.assertEqual(receipt["challenges"][0]["hypothesis"], "H1")
        self.assertEqual(
            [item["type"] for item in receipt["transitions"]],
            ["reopen", "revise"],
        )
        self.assertEqual(receipt["transitions"][0]["from"], "H1")
        self.assertEqual(receipt["transitions"][1]["from"], "H1")
        self.assertEqual(receipt["transitions"][1]["to"], "H2")
        self.assertEqual(receipt["evidence_metadata"]["e5"]["depends_on"], ["e3"])
        self.assertTrue(receipt["execution_receipts"])

    def test_unknown_runtime_evidence_is_preserved_as_adapter_failure(self):
        raw = {
            "episode_id": "EP_TEST",
            "configuration": "G1",
            "hypothesis_nodes": {"H1": 101, "H2": 102},
            "edge_to_evidence_ref": {"201": "e1"},
            "steps": [
                {
                    "step": 1,
                    "native_events": [
                        {
                            "source": "graphene_core",
                            "type": "terminal",
                            "hypothesis_node": 101,
                            "evidence_edges": [999],
                            "epistemic_state": "resolved",
                        }
                    ],
                }
            ],
        }
        receipt = adapt_v1.adapt_episode(raw)
        self.assertTrue(receipt["adapter_failure"])
        self.assertIn("unknown runtime evidence edge id: 999",
                      receipt["adapter_failure_detail"][0])

    def test_evaluator_excludes_adapter_failure_from_primary_metrics(self):
        task = {
            "id": "EP_TEST",
            "terminal_supported": "H2",
            "minimum_independent_families_for_resolution": 2,
            "events": [],
        }
        receipt = {
            "episode_id": "EP_TEST",
            "configuration": "G2",
            "decisions": [],
            "terminal_status": "open",
            "terminal_hypothesis": None,
            "evidence_refs": [],
            "hypotheses": [],
            "transitions": [],
            "adapter_failure": True,
            "adapter_failure_detail": ["native receipt missing"],
        }
        row = evaluate_v1.score_episode(task, receipt)
        self.assertTrue(row["adapter_failure"])
        self.assertTrue(row["excluded_from_primary_metrics"])
        self.assertNotIn("terminal_correct", row)


if __name__ == "__main__":
    unittest.main()
