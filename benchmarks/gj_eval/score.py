#!/usr/bin/env python3
"""Core scorer for GJ-Eval v1."""

from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import defaultdict
from typing import Any


def load_jsonl(path: str) -> list[dict[str, Any]]:
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise SystemExit(f"{path}:{line_no}: invalid JSON: {exc}") from exc
    return rows


def percentile(values: list[float], q: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    pos = (len(ordered) - 1) * q
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return ordered[lo]
    frac = pos - lo
    return ordered[lo] * (1 - frac) + ordered[hi] * frac


def ece(rows: list[tuple[float, int]], bins: int = 10) -> float | None:
    if not rows:
        return None
    total = len(rows)
    score = 0.0
    for b in range(bins):
        low = b / bins
        high = (b + 1) / bins
        bucket = [
            (c, y) for c, y in rows
            if low <= c < high or (b == bins - 1 and c == 1.0)
        ]
        if not bucket:
            continue
        conf = sum(c for c, _ in bucket) / len(bucket)
        acc = sum(y for _, y in bucket) / len(bucket)
        score += abs(conf - acc) * len(bucket) / total
    return score


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--predictions", required=True)
    args = parser.parse_args()

    worlds = {w["world_id"]: w for w in load_jsonl(args.worlds)}
    predictions = load_jsonl(args.predictions)
    by_system: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in predictions:
        by_system[row["system"]].append(row)

    report: dict[str, Any] = {"systems": {}}
    for system, rows in sorted(by_system.items()):
        rows = sorted(rows, key=lambda r: (r["world_id"], r["timestep"]))
        final_by_world: dict[str, dict[str, Any]] = {}
        correct_all = 0
        scored_all = 0
        confidences: list[tuple[float, int]] = []
        latencies: list[float] = []
        abstention_tp = abstention_fp = abstention_fn = 0
        wrong_dwell_total = 0
        recovery_latencies: list[int] = []
        premature_episodes = 0

        grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for row in rows:
            if row["world_id"] in worlds:
                grouped[row["world_id"]].append(row)

        for world_id, episode_rows in grouped.items():
            world = worlds[world_id]
            oracle = world["oracle"]
            truth = oracle["true_root"]
            decisive = int(oracle["decisive_timestep"])
            insufficient_until = int(oracle.get("insufficient_until", -1))
            episode_rows.sort(key=lambda r: r["timestep"])
            if episode_rows:
                final_by_world[world_id] = episode_rows[-1]

            committed_wrong_after_decisive = 0
            recovered_at = None
            had_premature = False

            for row in episode_rows:
                t = int(row["timestep"])
                choice = row.get("root_choice")
                act = row.get("act")
                correct = int(choice == truth)
                scored_all += 1
                correct_all += correct

                confidence = float(row.get("selected_confidence", 0.0))
                confidences.append((confidence, correct))
                latencies.append(float(row.get("latency_ms", 0.0)))

                should_abstain = t <= insufficient_until
                did_abstain = adapter_ok and (act in ("abstain", "review") or choice in (None, "unknown"))
                if did_abstain and should_abstain:
                    abstention_tp += 1
                elif did_abstain and not should_abstain:
                    abstention_fp += 1
                elif (not did_abstain) and should_abstain:
                    abstention_fn += 1

                if t <= insufficient_until:
                    if choice not in (None, "unknown") and confidence >= 0.75 and choice != truth:
                        had_premature = True

                if t >= decisive:
                    if adapter_ok and (choice == truth or did_abstain):
                        if recovered_at is None:
                            recovered_at = t
                    elif choice not in (None, "unknown"):
                        committed_wrong_after_decisive += 1

            if had_premature:
                premature_episodes += 1
            wrong_dwell_total += committed_wrong_after_decisive
            if recovered_at is not None:
                recovery_latencies.append(max(0, recovered_at - decisive))

        final_correct = sum(
            1 for wid, row in final_by_world.items()
            if row.get("root_choice") == worlds[wid]["oracle"]["true_root"]
        )
        final_n = len(final_by_world)
        abstention_precision = (
            abstention_tp / (abstention_tp + abstention_fp)
            if (abstention_tp + abstention_fp) else None
        )
        abstention_recall = (
            abstention_tp / (abstention_tp + abstention_fn)
            if (abstention_tp + abstention_fn) else None
        )

        report["systems"][system] = {
            "worlds_scored": final_n,
            "predictions_scored": scored_all,
            "final_accuracy": final_correct / final_n if final_n else None,
            "trajectory_accuracy": correct_all / scored_all if scored_all else None,
            "wrong_model_dwell_time_total": wrong_dwell_total,
            "wrong_model_dwell_time_mean": wrong_dwell_total / final_n if final_n else None,
            "recovery_latency_mean": (
                statistics.mean(recovery_latencies) if recovery_latencies else None
            ),
            "premature_convergence_episode_rate": (
                premature_episodes / final_n if final_n else None
            ),
            "abstention_precision": abstention_precision,
            "abstention_recall": abstention_recall,
            "selected_confidence_ece_10": ece(confidences, 10),
            "latency_p50_ms": percentile(latencies, 0.50),
            "latency_p95_ms": percentile(latencies, 0.95),
            "latency_p99_ms": percentile(latencies, 0.99),
        }

    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
