#!/usr/bin/env python3
"""Mechanical validation for authorized V2 score-bearing raw evidence."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping

CONFIGS = {"C0", "G0E", "C1", "G1", "G2"}
FORBIDDEN_KEYS = {
    "oracle",
    "terminal_operative",
    "terminal_committed",
    "response_window",
    "challenge_warranted_steps",
    "minimum_independent_families_for_resolution",
    "terminal_supported",
    "expected_answer",
    "benchmark_correctness",
    "decisive",
    "independent",
    "evaluator_independence_label",
    "future_observations",
    "task_family",
    "mirror_primary",
}


def nested_keys(value: Any):
    if isinstance(value, Mapping):
        for key, item in value.items():
            yield str(key)
            yield from nested_keys(item)
    elif isinstance(value, list):
        for item in value:
            yield from nested_keys(item)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw")
    args = parser.parse_args()
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))

    assert raw["schema"] == "epistemic-process-v2-score-raw-v1"
    assert raw["experiment_id"] == "EP-PROCESS-V2-SCORE-001"
    assert raw["score_bearing"] is True
    assert raw["score_bearing_authorized"] is True
    assert raw["seed"] == 20261005
    assert set(raw["configurations"]) == CONFIGS
    assert len(raw["configurations"]) == 5

    leaked = set(nested_keys(raw)) & FORBIDDEN_KEYS
    if leaked:
        raise AssertionError(
            f"oracle/evaluator fields leaked into score runtime evidence: "
            f"{sorted(leaked)}"
        )

    episodes = raw["episodes"]
    assert len(episodes) == 270 * 5
    seen = set()
    failures = 0
    gaps = 0
    step_records = 0
    recovery_rounds = 0
    per_config = {config: 0 for config in CONFIGS}
    for episode in episodes:
        pair = (episode["episode_id"], episode["configuration"])
        if pair in seen:
            raise AssertionError(f"duplicate score episode/config pair {pair}")
        seen.add(pair)
        config = episode["configuration"]
        assert config in CONFIGS
        per_config[config] += 1
        assert episode.get("score_bearing") is True
        assert episode.get("score_bearing_authorized") is True
        failures += bool(episode.get("failures"))
        for step in episode.get("steps") or []:
            step_records += 1
            gaps += len(step.get("telemetry_gaps") or [])
            recovery_rounds += len(step.get("recovery_rounds") or [])

    assert per_config == {config: 270 for config in CONFIGS}
    if failures:
        raise AssertionError(f"runtime failures present: {failures}")
    if gaps:
        raise AssertionError(f"telemetry gaps present: {gaps}")

    print(json.dumps({
        "score_raw_validation": "passed",
        "configuration_episode_total": len(episodes),
        "per_config": dict(sorted(per_config.items())),
        "runtime_failures": failures,
        "telemetry_gaps": gaps,
        "step_records": step_records,
        "recovery_rounds": recovery_rounds,
        "score_bearing": True,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
