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
