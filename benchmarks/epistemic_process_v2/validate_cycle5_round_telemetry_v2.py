#!/usr/bin/env python3
"""Validate Cycle-5 recovery-round telemetry without evaluating correctness."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

REQUIRED_ROUND = {
    "round_index", "trigger", "options_before", "options_after",
    "bundle_hash_before", "bundle_hash_after",
    "visited_states_before", "visited_states_after", "frontier_changed",
    "has_answer_before", "has_answer_after",
    "operative_hypothesis_before", "operative_hypothesis_after",
    "committed_answer_before", "committed_answer_after",
    "status_before", "status_after",
    "target_ranking_before", "target_ranking_after",
    "rank_changed", "operative_hypothesis_changed",
    "status_changed", "committed_answer_changed", "stop_reason",
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw")
    args = parser.parse_args()
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))
    assert raw["score_bearing"] is False
    assert raw["score_bearing_authorized"] is False

    rounds = 0
    gaps = 0
    configs = set()
    failures = 0
    for episode in raw["episodes"]:
        configs.add(episode["configuration"])
        failures += bool(episode.get("failures"))
        for step in episode.get("steps") or []:
            gaps += len(step.get("telemetry_gaps") or [])
            for row in step.get("recovery_rounds") or []:
                rounds += 1
                missing = REQUIRED_ROUND - set(row)
                if missing:
                    raise AssertionError(
                        f"{episode['configuration']}/{episode['episode_id']}/"
                        f"{step['step']}: missing {sorted(missing)}"
                    )
                assert isinstance(row["target_ranking_before"], list)
                assert isinstance(row["target_ranking_after"], list)
                for boolean in (
                    "frontier_changed", "rank_changed",
                    "operative_hypothesis_changed", "status_changed",
                    "committed_answer_changed",
                ):
                    assert isinstance(row[boolean], bool)

    assert failures == 0
    assert gaps == 0
    assert rounds > 0
    assert configs == {"C0", "G0E", "C1", "G1", "G2"}
    print(json.dumps({
        "cycle5_round_telemetry": "passed",
        "recovery_rounds_observed": rounds,
        "telemetry_gaps": gaps,
        "runtime_failures": failures,
        "score_bearing": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
