#!/usr/bin/env python3
"""Authorized score-bearing executor for EP-PROCESS-V2.

This file deliberately reuses the Cycle-4/Cycle-5 execution functions rather
than implementing a second runtime path. Authorization is established by the
workflow's score_gate_v2.py step before this script is called.
"""
from __future__ import annotations

import argparse
import json
import tempfile
import time
from pathlib import Path
from typing import Sequence

from run_cycle4_unscored_v2 import (
    CONFIGS,
    c0_episode,
    dump,
    load,
    runtime_episode,
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--runtime-runner", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--experiment-id", required=True)
    args = parser.parse_args()

    if args.experiment_id != "EP-PROCESS-V2-SCORE-001":
        raise SystemExit("unexpected experiment id")
    if args.seed != 20261005:
        raise SystemExit("unexpected runtime seed")

    tasks = load(args.tasks)
    runtime_runner = Path(args.runtime_runner).resolve()
    if not runtime_runner.exists():
        raise SystemExit(f"runtime runner does not exist: {runtime_runner}")

    configs: Sequence[str] = CONFIGS
    episodes = []
    started = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="moolbase-v2-score-") as td:
        work_root = Path(td)
        for config in configs:
            for task in tasks["episodes"]:
                if config == "C0":
                    episode = c0_episode(task)
                else:
                    episode = runtime_episode(
                        task,
                        config,
                        runtime_runner,
                        args.seed,
                        work_root,
                    )
                episode["score_bearing"] = True
                episode["score_bearing_authorized"] = True
                episodes.append(episode)

    raw = {
        "schema": "epistemic-process-v2-score-raw-v1",
        "experiment_id": args.experiment_id,
        "mode": "score-bearing",
        "score_bearing": True,
        "score_bearing_authorized": True,
        "seed": args.seed,
        "configurations": list(configs),
        "runtime_input_fields": [
            "step", "id", "family", "kind", "bears_on",
            "depends_on", "revokes",
        ],
        "episodes": episodes,
        "execution_seconds": time.perf_counter() - started,
    }
    dump(args.out, raw)

    failures = sum(bool(ep.get("failures")) for ep in episodes)
    print(json.dumps({
        "experiment_id": args.experiment_id,
        "configuration_episode_total": len(episodes),
        "runtime_failures": failures,
        "score_bearing": True,
    }, sort_keys=True))
    if failures:
        raise SystemExit("score-bearing runtime failures observed; no retry")


if __name__ == "__main__":
    main()
