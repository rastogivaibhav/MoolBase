#!/usr/bin/env python3
"""Cluster-paired bootstrap comparison for GJ-Eval primary endpoints."""

from __future__ import annotations

import argparse
import json
import random
from collections import defaultdict
from typing import Any


def load_jsonl(path: str) -> list[dict[str, Any]]:
    with open(path, "r", encoding="utf-8") as handle:
        return [json.loads(line) for line in handle if line.strip()]


def episode_metrics(world: dict[str, Any], rows: list[dict[str, Any]]) -> dict[str, float]:
    by_t = {int(row["timestep"]): row for row in rows}
    expected = [int(step["timestep"]) for step in world["timeline"]]
    missing = [t for t in expected if t not in by_t]
    if missing:
        raise ValueError(f"{world['world_id']}: missing predictions at {missing}")

    truth = world["oracle"]["true_root"]
    decisive = int(world["oracle"]["decisive_timestep"])
    insufficient_until = int(world["oracle"].get("insufficient_until", -1))
    final_t = max(expected)
    final = by_t[final_t]
    final_ok = final.get("adapter_status") == "ok"
    final_accuracy = float(final_ok and final.get("root_choice") == truth)

    wrong_dwell = 0.0
    premature = 0.0
    recovered_at = None
    for t in expected:
        row = by_t[t]
        ok = row.get("adapter_status") == "ok"
        choice = row.get("root_choice")
        act = row.get("act")
        confidence = float(row.get("selected_confidence", 0.0))

        if (
            t <= insufficient_until
            and ok
            and act == "act"
            and choice not in (None, "unknown", truth)
            and confidence >= 0.75
        ):
            premature = 1.0

        if t >= decisive:
            if ok and choice == truth and recovered_at is None:
                recovered_at = t
            elif ok and act == "act" and choice not in (None, "unknown", truth):
                wrong_dwell += 1.0

    recovery = (
        float(recovered_at - decisive)
        if recovered_at is not None
        else float(final_t - decisive + 1)
    )
    return {
        "final_accuracy": final_accuracy,
        "wrong_model_dwell_time": wrong_dwell,
        "recovery_latency": recovery,
        "premature_convergence_rate": premature,
    }


def percentile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    pos = (len(ordered) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(ordered) - 1)
    frac = pos - lo
    return ordered[lo] * (1.0 - frac) + ordered[hi] * frac


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--predictions", required=True)
    parser.add_argument("--system-a", required=True)
    parser.add_argument("--system-b", required=True)
    parser.add_argument("--bootstrap", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=20260921)
    args = parser.parse_args()

    worlds = {w["world_id"]: w for w in load_jsonl(args.worlds)}
    rows = load_jsonl(args.predictions)
    grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        if row.get("system") in {args.system_a, args.system_b}:
            grouped[(row["system"], row["world_id"])].append(row)

    per_world: dict[str, dict[str, dict[str, float]]] = {}
    for wid, world in worlds.items():
        per_world[wid] = {}
        for system in (args.system_a, args.system_b):
            key = (system, wid)
            if key not in grouped:
                raise SystemExit(f"missing system/world predictions: {system} {wid}")
            per_world[wid][system] = episode_metrics(world, grouped[key])

    clusters: dict[str, list[str]] = defaultdict(list)
    for wid, world in worlds.items():
        clusters[str(world["scenario_id"])].append(wid)
    cluster_ids = sorted(clusters)

    def advantage(metric: str, wid: str) -> float:
        a = per_world[wid][args.system_a][metric]
        b = per_world[wid][args.system_b][metric]
        if metric == "final_accuracy":
            return a - b
        return b - a

    metrics = [
        "final_accuracy",
        "wrong_model_dwell_time",
        "recovery_latency",
        "premature_convergence_rate",
    ]
    observed = {
        metric: mean([advantage(metric, wid) for wid in worlds])
        for metric in metrics
    }

    rng = random.Random(args.seed)
    samples: dict[str, list[float]] = {metric: [] for metric in metrics}
    for _ in range(args.bootstrap):
        picked = [rng.choice(cluster_ids) for _ in cluster_ids]
        sampled_worlds = [wid for cid in picked for wid in clusters[cid]]
        for metric in metrics:
            samples[metric].append(
                mean([advantage(metric, wid) for wid in sampled_worlds])
            )

    report = {
        "comparison": {
            "system_a": args.system_a,
            "system_b": args.system_b,
            "interpretation": "positive advantage favors system_a",
            "bootstrap_unit": "scenario_id",
            "bootstrap_samples": args.bootstrap,
            "seed": args.seed,
        },
        "metrics": {},
    }
    for metric in metrics:
        low = percentile(samples[metric], 0.025)
        high = percentile(samples[metric], 0.975)
        report["metrics"][metric] = {
            "advantage": observed[metric],
            "ci95": [low, high],
            "measurable_advantage_for_system_a": low > 0.0,
            "measurable_advantage_for_system_b": high < 0.0,
        }

    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
