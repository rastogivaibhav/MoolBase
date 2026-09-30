#!/usr/bin/env python3
"""Emit only mechanical Cycle-5 facts; intentionally no accuracy/outcome metrics."""
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))

    by_config = collections.Counter()
    failures = 0
    gaps = 0
    recovery_rounds = 0
    steps = 0
    for episode in raw["episodes"]:
        by_config[episode["configuration"]] += 1
        failures += bool(episode.get("failures"))
        for step in episode.get("steps") or []:
            steps += 1
            gaps += len(step.get("telemetry_gaps") or [])
            recovery_rounds += len(step.get("recovery_rounds") or [])

    summary = {
        "schema": "epistemic-process-v2-cycle5-mechanical-summary-v1",
        "score_bearing": False,
        "outcome_metrics_computed": False,
        "oracle_join_performed": False,
        "configuration_episode_counts": dict(sorted(by_config.items())),
        "configuration_episode_total": sum(by_config.values()),
        "runtime_failures": failures,
        "telemetry_gaps": gaps,
        "step_records": steps,
        "recovery_rounds": recovery_rounds,
    }
    if failures or gaps:
        raise SystemExit(json.dumps(summary, sort_keys=True))
    Path(args.out).write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
