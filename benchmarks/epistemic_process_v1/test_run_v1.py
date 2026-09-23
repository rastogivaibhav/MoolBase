#!/usr/bin/env python3
import unittest

import run_v1


TASK = {
    "id": "EP_TEST",
    "initial_preference": "H1",
    "terminal_supported": "H2",
    "minimum_independent_families_for_resolution": 2,
    "events": [
        {"step": 1, "id": "e1", "family": "F_A", "kind": "support", "bears_on": "H1", "independent": True, "decisive": False},
        {"step": 2, "id": "e2", "family": "F_B", "kind": "support", "bears_on": "H2", "independent": True, "decisive": False},
        {"step": 3, "id": "e3", "family": "F_C", "kind": "refute", "bears_on": "H1", "independent": True, "decisive": True},
        {"step": 4, "id": "e4", "family": "F_D", "kind": "support", "bears_on": "H2", "independent": True, "decisive": False},
    ],
}


class RunnerContractTests(unittest.TestCase):
    def test_b0_g0_emit_no_native_semantic_events(self):
        for config in ("B0", "G0"):
            episode = run_v1.run_episode(TASK, config)
            self.assertTrue(all("decision" in step for step in episode["steps"]))
            self.assertTrue(all("native_events" not in step for step in episode["steps"]))

    def test_g1_never_emits_dwm_source_or_reopen(self):
        episode = run_v1.run_episode(TASK, "G1")
        events = [e for step in episode["steps"] for e in step["native_events"]]
        self.assertFalse(any(e.get("source") == "dwm" for e in events))
        self.assertFalse(any(e.get("type") in {"challenge", "reopen"} for e in events))
        self.assertTrue(any(e.get("type") == "revision" for e in events))

    def test_g2_emits_challenge_reopen_and_revision_after_decisive_refutation(self):
        episode = run_v1.run_episode(TASK, "G2")
        step3 = episode["steps"][2]["native_events"]
        kinds = [e["type"] for e in step3]
        self.assertIn("challenge", kinds)
        self.assertIn("reopen", kinds)
        # Revision is delayed until replacement evidence reaches the preregistered
        # independent-family threshold, so it must not be manufactured at step 3.
        self.assertNotIn("revision", kinds)
        step4 = episode["steps"][3]["native_events"]
        self.assertIn("revision", [e["type"] for e in step4])

    def test_event_order_and_identity_are_preserved(self):
        episode = run_v1.run_episode(TASK, "G2")
        self.assertEqual([s["step"] for s in episode["steps"]], [1, 2, 3, 4])
        self.assertEqual(episode["edge_to_evidence_ref"], {
            "1001": "e1", "1002": "e2", "1003": "e3", "1004": "e4"
        })


if __name__ == "__main__":
    unittest.main()
