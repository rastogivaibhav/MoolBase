#!/usr/bin/env python3
"""Post-run diagnostic analysis for GJ-Eval artifacts.

This script is intentionally analysis-only. It does not change benchmark
generation, adapter behavior, thresholds, or scoring semantics.
"""

from __future__ import annotations

import argparse
import json
import math
import statistics
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


def load_jsonl(path: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for line_no, line in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"{path}:{line_no}: invalid JSON: {exc}") from exc
        if not isinstance(value, dict):
            raise SystemExit(f"{path}:{line_no}: expected JSON object")
        rows.append(value)
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
    return ordered[lo] * (1.0 - frac) + ordered[hi] * frac


def episode_metrics(
    world: dict[str, Any],
    rows: list[dict[str, Any]],
) -> dict[str, Any]:
    truth = str(world["oracle"]["true_root"])
    decisive = int(world["oracle"]["decisive_timestep"])
    insufficient_until = int(world["oracle"].get("insufficient_until", -1))
    expected_ts = [int(step["timestep"]) for step in world["timeline"]]
    by_t = {int(row["timestep"]): row for row in rows}
    missing = [t for t in expected_ts if t not in by_t]

    status_counts = Counter(str(row.get("adapter_status", "missing")) for row in rows)
    all_ok = not missing and all(by_t[t].get("adapter_status") == "ok" for t in expected_ts)

    final_t = max(expected_ts)
    final = by_t.get(final_t)
    final_choice = final.get("root_choice") if final else None
    final_act = final.get("act") if final else None
    final_ok = bool(final and final.get("adapter_status") == "ok")
    final_correct = bool(final_ok and final_choice == truth)
    final_abstain = bool(
        final_ok and (
            final_choice in (None, "unknown")
            or final_act in ("abstain", "review")
        )
    )
    final_false_root = bool(
        final_ok
        and final_choice not in (None, "unknown", truth)
    )

    correct_steps = 0
    ok_steps = 0
    wrong_dwell = 0
    recovered_at: int | None = None
    premature = False
    latency: list[float] = []

    for t in expected_ts:
        row = by_t.get(t)
        if row is None or row.get("adapter_status") != "ok":
            continue
        ok_steps += 1
        choice = row.get("root_choice")
        act = row.get("act")
        confidence = float(row.get("selected_confidence", 0.0))
        latency.append(float(row.get("latency_ms", 0.0)))
        if choice == truth:
            correct_steps += 1

        if (
            t <= insufficient_until
            and act == "act"
            and choice not in (None, "unknown", truth)
            and confidence >= 0.75
        ):
            premature = True

        if t >= decisive:
            if choice == truth and recovered_at is None:
                recovered_at = t
            elif (
                act == "act"
                and choice not in (None, "unknown", truth)
            ):
                wrong_dwell += 1

    recovery_latency = (
        recovered_at - decisive
        if recovered_at is not None
        else final_t - decisive + 1
    )

    # Hypothesis-survival diagnostics use any ranked_hypotheses exposed by the
    # adapter. No ranking is fabricated for systems that do not provide one.
    survival: dict[int, bool] = {}
    for t, row in by_t.items():
        ranked = row.get("ranked_hypotheses")
        if row.get("adapter_status") == "ok" and isinstance(ranked, list):
            survival[t] = truth in [str(x) for x in ranked]

    return {
        "all_ok": all_ok,
        "missing_timesteps": missing,
        "status_counts": dict(status_counts),
        "final_correct": final_correct,
        "final_abstain": final_abstain,
        "final_false_root": final_false_root,
        "final_choice": final_choice,
        "final_act": final_act,
        "trajectory_accuracy": (
            correct_steps / len(expected_ts) if expected_ts else None
        ),
        "wrong_model_dwell_time": wrong_dwell,
        "recovery_latency": recovery_latency,
        "premature_convergence": premature,
        "latencies": latency,
        "hypothesis_survival": survival,
    }


def summarize_bucket(episodes: list[dict[str, Any]]) -> dict[str, Any]:
    n = len(episodes)
    if n == 0:
        return {"worlds": 0}

    final_correct = sum(bool(e["final_correct"]) for e in episodes)
    final_abstain = sum(bool(e["final_abstain"]) for e in episodes)
    final_false = sum(bool(e["final_false_root"]) for e in episodes)
    resolved = final_correct + final_false
    all_ok = sum(bool(e["all_ok"]) for e in episodes)
    latencies = [x for e in episodes for x in e["latencies"]]
    trajectory = [
        float(e["trajectory_accuracy"])
        for e in episodes
        if e["trajectory_accuracy"] is not None
    ]
    recoveries = [float(e["recovery_latency"]) for e in episodes]
    wrong_dwell = [float(e["wrong_model_dwell_time"]) for e in episodes]
    premature = sum(bool(e["premature_convergence"]) for e in episodes)

    survival_by_t: dict[int, list[bool]] = defaultdict(list)
    for episode in episodes:
        for t, survived in episode["hypothesis_survival"].items():
            survival_by_t[int(t)].append(bool(survived))

    status_counts: Counter[str] = Counter()
    for episode in episodes:
        status_counts.update(episode["status_counts"])

    return {
        "worlds": n,
        "all_prediction_rows_ok_world_rate": all_ok / n,
        "adapter_status_counts": dict(status_counts),
        "final_accuracy": final_correct / n,
        "final_abstention_or_review_rate": final_abstain / n,
        "final_false_root_rate": final_false / n,
        "final_resolution_rate": resolved / n,
        "selective_accuracy_when_root_named": (
            final_correct / resolved if resolved else None
        ),
        "trajectory_accuracy_mean": (
            statistics.mean(trajectory) if trajectory else None
        ),
        "wrong_model_dwell_time_mean": (
            statistics.mean(wrong_dwell) if wrong_dwell else None
        ),
        "recovery_latency_mean": (
            statistics.mean(recoveries) if recoveries else None
        ),
        "premature_convergence_rate": premature / n,
        "latency_p50_ms": percentile(latencies, 0.50),
        "latency_p95_ms": percentile(latencies, 0.95),
        "latency_p99_ms": percentile(latencies, 0.99),
        "true_hypothesis_survival_by_timestep": {
            str(t): sum(values) / len(values)
            for t, values in sorted(survival_by_t.items())
            if values
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--predictions", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()

    worlds = {w["world_id"]: w for w in load_jsonl(args.worlds)}
    rows = load_jsonl(args.predictions)

    grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    systems: set[str] = set()
    for row in rows:
        system = str(row.get("system", "unknown"))
        wid = str(row.get("world_id", ""))
        systems.add(system)
        if wid in worlds:
            grouped[(system, wid)].append(row)

    report: dict[str, Any] = {
        "systems": {},
        "validation": {
            "world_count": len(worlds),
            "systems": sorted(systems),
        },
    }

    for system in sorted(systems):
        overall: list[dict[str, Any]] = []
        by_variant: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for wid, world in worlds.items():
            episode = episode_metrics(world, grouped.get((system, wid), []))
            overall.append(episode)
            by_variant[str(world["variant"])].append(episode)

        report["systems"][system] = {
            "overall": summarize_bucket(overall),
            "by_variant": {
                variant: summarize_bucket(episodes)
                for variant, episodes in sorted(by_variant.items())
            },
        }

    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        Path(args.output).write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
