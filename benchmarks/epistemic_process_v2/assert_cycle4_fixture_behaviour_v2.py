#!/usr/bin/env python3
"""Mechanical fixture assertions for Cycle 4.

These are harness-contract checks, not score-bearing metrics.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True)
    parser.add_argument("--tasks", required=True)
    args = parser.parse_args()

    raw = load(Path(args.raw))
    tasks = load(Path(args.tasks))
    if raw.get("score_bearing") is not False:
        raise AssertionError("fixture assertions may only run unscored")

    task_refs = {
        str(task["id"]): {str(event["id"]) for event in task["events"]}
        for task in tasks["episodes"]
    }
    episodes = {
        (str(ep["episode_id"]), str(ep["configuration"])): ep
        for ep in raw["episodes"]
    }

    expected_common_head_final = {
        ("C4_HISTORY_BEATS_CURRENT_01", "C0"): "H2",
        ("C4_HISTORY_BEATS_CURRENT_01", "C1"): "H1",
        ("C4_DUPLICATE_FAMILY_02", "C1"): None,
        ("C4_LATE_REFUTATION_03", "C1"): "H2",
        ("C4_TIE_ABSTAINS_04", "C1"): None,
        ("C4_CURRENT_ONLY_DIFFERS_05", "C0"): "H2",
        ("C4_CURRENT_ONLY_DIFFERS_05", "C1"): "H1",
        ("C4_ISOLATED_EPISODE_06", "C1"): "H2",
    }
    for key, expected in expected_common_head_final.items():
        ep = episodes[key]
        actual = ep["steps"][-1]["final_state"]["operative_hypothesis"]
        if actual != expected:
            raise AssertionError(
                f"{key}: expected final operative={expected!r}, got {actual!r}"
            )

    for task_id, allowed in task_refs.items():
        g0e = episodes[(task_id, "G0E")]
        c1 = episodes[(task_id, "C1")]
        for step_g0e, step_c1 in zip(g0e["steps"], c1["steps"]):
            refs_g0e = set(step_g0e["active_evidence_refs"])
            refs_c1 = set(step_c1["active_evidence_refs"])
            if not refs_g0e <= allowed:
                raise AssertionError(
                    f"{task_id}: G0E cross-episode evidence leakage {refs_g0e-allowed}"
                )
            if refs_c1 != refs_g0e:
                raise AssertionError(
                    f"{task_id}/step {step_g0e['step']}: "
                    "C1 did not consume exactly the G0E evidence projection"
                )
        final_refs = set(g0e["steps"][-1]["active_evidence_refs"])
        if final_refs != allowed:
            raise AssertionError(
                f"{task_id}: G0E final retention mismatch "
                f"expected={sorted(allowed)} actual={sorted(final_refs)}"
            )

    print(
        json.dumps(
            {
                "cycle4_fixture_behaviour": "passed",
                "history_control_separation": True,
                "duplicate_family_neutrality": True,
                "g0e_episode_retention": "exact",
                "cross_episode_leakage": 0,
                "score_bearing": False,
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
