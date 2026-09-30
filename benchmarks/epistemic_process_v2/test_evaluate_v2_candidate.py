#!/usr/bin/env python3
from __future__ import annotations

from evaluate_v2_candidate import evaluate


def state(op, committed, families=None, derivations=None):
    return {
        "operative_hypothesis": op,
        "committed_answer": committed,
        "evidence_family_ids": families or [],
        "dependency_lineage_ids": derivations or [],
    }


def main():
    task = {
        "id": "T1",
        "challenge_warranted_steps": [2],
        "oracle": {
            "terminal_operative": "H2",
            "terminal_committed": "H2",
            "response_window": {
                "trigger_step": 2,
                "contradicted": "H1",
                "acceptable_to": ["H2", None],
                "max_latency_steps": 0,
            },
        },
        "events": [
            {"step": 1, "id": "e1", "family": "F1", "kind": "support",
             "bears_on": "H1"},
            {"step": 2, "id": "e2", "family": "F2", "kind": "refute",
             "bears_on": "H1", "decisive": True},
        ],
    }
    raw = {
        "score_bearing": False,
        "configurations": ["G2"],
        "episodes": [{
            "episode_id": "T1",
            "configuration": "G2",
            "evidence_metadata": {
                "e1": {"family": "F1", "kind": "support",
                       "bears_on": "H1", "depends_on": []},
                "e2": {"family": "F2", "kind": "refute",
                       "bears_on": "H1", "depends_on": []},
            },
            "steps": [
                {
                    "step": 1,
                    "active_evidence_refs": ["e1"],
                    "final_state": state("H1", "H1", ["F1"]),
                    "native_events": [],
                    "recovery_rounds": [],
                },
                {
                    "step": 2,
                    "active_evidence_refs": ["e1", "e2"],
                    "final_state": state("H2", "H2", ["F1", "F2"]),
                    "native_events": [
                        {"type": "challenge"},
                        {"type": "reopen"},
                        {"type": "revision"},
                    ],
                    "recovery_rounds": [{
                        "trigger": "dwm_opposition",
                        "frontier_changed": True,
                        "rank_changed": True,
                        "operative_hypothesis_changed": True,
                        "status_changed": True,
                        "committed_answer_changed": True,
                    }],
                },
            ],
        }],
    }
    result = evaluate({"episodes": [task]}, raw)["G2"]
    assert result["operative_hypothesis_accuracy"] == 1.0
    assert result["committed_accuracy"] == 1.0
    assert result["cross_step_refutation_response_rate"] == 1.0
    assert result["challenge_precision"] == 1.0
    assert result["unnecessary_reopen_rate"] == 0.0
    assert result["reopen_usefulness_rate"] == 1.0
    assert result["within_call_revision_rate"] == 1.0
    print("cycle5_evaluator_contracts=passed")


if __name__ == "__main__":
    main()
