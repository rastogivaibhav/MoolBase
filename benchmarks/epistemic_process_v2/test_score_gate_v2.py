#!/usr/bin/env python3
from __future__ import annotations

import copy

from score_gate_v2 import ScoreAuthorizationError, validate_manifest_contract


def base():
    return {
        "schema": "epistemic-process-v2-score-freeze-v1",
        "status": "FROZEN",
        "experiment_id": "EP-PROCESS-V2-SCORE-001",
        "score_bearing_authorized": True,
        "candidate_commit": "a" * 40,
        "candidate_tree": "b" * 40,
        "evaluator_freeze_commit": "c" * 40,
        "evaluator_freeze_manifest_sha256": "d" * 64,
        "allowed_changes_after_candidate": [
            "benchmarks/epistemic_process_v2/score_freeze_v2.json"
        ],
        "profiles": ["C0", "G0E", "C1", "G1", "G2"],
        "score_repetitions": 1,
        "automatic_retry": False,
        "imputation": False,
        "task_episodes": 270,
        "task_families": 9,
        "episodes_per_family": 30,
        "runtime_seed": 20261005,
        "bootstrap_seed": 20261006,
        "bootstrap_resamples": 10000,
        "policies": {
            "candidate_outcomes_previously_opened": False,
            "candidate_oracle_join_previously_performed": False,
            "post_freeze_tuning_allowed": False,
            "failed_episode_imputation": False,
            "score_run_retry": False,
        },
    }


def rejected(mutator):
    manifest = base()
    mutator(manifest)
    try:
        validate_manifest_contract(manifest)
    except ScoreAuthorizationError:
        return
    raise AssertionError("invalid score manifest was accepted")


def main():
    validate_manifest_contract(base())
    rejected(lambda m: m.__setitem__("status", "DRAFT"))
    rejected(lambda m: m.__setitem__("score_bearing_authorized", False))
    rejected(lambda m: m.__setitem__("score_repetitions", 2))
    rejected(lambda m: m.__setitem__("automatic_retry", True))
    rejected(lambda m: m.__setitem__("task_episodes", 240))
    rejected(lambda m: m.__setitem__("runtime_seed", 1))
    rejected(lambda m: m["profiles"].reverse())
    rejected(lambda m: m["allowed_changes_after_candidate"].append("x"))
    rejected(
        lambda m: m["policies"].__setitem__(
            "candidate_outcomes_previously_opened", True
        )
    )
    rejected(
        lambda m: m["policies"].__setitem__("post_freeze_tuning_allowed", True)
    )
    print("cycle7_score_gate_contracts=passed")


if __name__ == "__main__":
    main()
