#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean, median
from typing import Any, Iterable

EPSILON = 1e-9


def as_float(row: dict[str, str], key: str) -> float:
    return float(row[key])


def as_bool(row: dict[str, str], key: str) -> bool:
    return row[key] == "1"


def rate(values: Iterable[bool]) -> float:
    materialised = list(values)
    return sum(materialised) / len(materialised) if materialised else math.nan


def group_metrics(rows: list[dict[str, str]]) -> dict[str, Any]:
    energies = {
        condition: [as_float(row, f"{condition}_energy") for row in rows]
        for condition in ("gold", "missing", "contradiction", "duplicate", "distractor", "wrong")
    }
    metrics: dict[str, Any] = {
        "examples": len(rows),
        "energy_mean": {key: mean(values) for key, values in energies.items()},
        "energy_median": {key: median(values) for key, values in energies.items()},
        "missing_hop_detection_rate": rate(
            as_float(row, "missing_energy") > as_float(row, "gold_energy") + EPSILON
            for row in rows
        ),
        "contradiction_detection_rate": rate(
            as_float(row, "contradiction_energy") > as_float(row, "gold_energy") + EPSILON
            for row in rows
        ),
        "duplicate_non_improvement_rate": rate(
            as_float(row, "duplicate_energy") >= as_float(row, "gold_energy") - EPSILON
            for row in rows
        ),
        "distractor_non_improvement_rate": rate(
            as_float(row, "distractor_energy") >= as_float(row, "gold_energy") - EPSILON
            for row in rows
        ),
        "wrong_complete_separation_rate": rate(
            abs(as_float(row, "wrong_energy") - as_float(row, "gold_energy")) > EPSILON
            for row in rows
        ),
        "repair_monotonic_rate": rate(as_bool(row, "repair_monotonic") for row in rows),
        "repair_goal_rate": rate(as_bool(row, "repair_goal_reached") for row in rows),
        "repair_convergence_rate": rate(as_bool(row, "repair_convergence") for row in rows),
        "stable_rate": {
            condition: rate(as_bool(row, f"{condition}_stable") for row in rows)
            for condition in ("gold", "missing", "contradiction", "duplicate", "distractor", "wrong")
        },
        "duplicate_independence_inflation_rate": rate(
            int(row["duplicate_independent"]) > int(row["gold_independent"])
            for row in rows
        ),
        "duplicate_pattern_lock_reduction_rate": rate(
            as_float(row, "duplicate_pattern_lock") + EPSILON < as_float(row, "gold_pattern_lock")
            for row in rows
        ),
        "distractor_pattern_lock_reduction_rate": rate(
            as_float(row, "distractor_pattern_lock") + EPSILON < as_float(row, "gold_pattern_lock")
            for row in rows
        ),
    }
    return metrics


def percent(value: float) -> str:
    return "n/a" if math.isnan(value) else f"{100.0 * value:.1f}%"


def render_markdown(summary: dict[str, Any]) -> str:
    overall = summary["overall"]
    lines = [
        "# 2WikiMultiHopQA Lyapunov Critic Benchmark",
        "",
        f"Examples: **{overall['examples']}**",
        "",
        "## Discrimination results",
        "",
        "| Diagnostic | Result | Initial target |",
        "|---|---:|---:|",
        f"| Missing-hop energy above gold | {percent(overall['missing_hop_detection_rate'])} | >=90% |",
        f"| Contradiction energy above gold | {percent(overall['contradiction_detection_rate'])} | >=90% |",
        f"| Same-source duplicate not rewarded | {percent(overall['duplicate_non_improvement_rate'])} | >=98% |",
        f"| Irrelevant distractor not rewarded | {percent(overall['distractor_non_improvement_rate'])} | >=95% |",
        f"| Repair trajectory monotonic | {percent(overall['repair_monotonic_rate'])} | >=90% |",
        "",
        "## Boundary and failure diagnostics",
        "",
        f"- Gold bundles classified stable: **{percent(overall['stable_rate']['gold'])}**. 2Wiki normally supplies one canonical evidence chain, not independent corroboration, so low stability is not automatically a defect.",
        f"- Structurally complete wrong bundles separated from gold: **{percent(overall['wrong_complete_separation_rate'])}**. A low result confirms that the critic measures epistemic structure, not semantic truth.",
        f"- Same-source duplicates counted as additional independent paths: **{percent(overall['duplicate_independence_inflation_rate'])}**.",
        f"- Same-source duplicates reduced pattern-lock score: **{percent(overall['duplicate_pattern_lock_reduction_rate'])}**.",
        f"- Irrelevant distractors reduced pattern-lock score: **{percent(overall['distractor_pattern_lock_reduction_rate'])}**.",
        "",
        "## Mean Lyapunov energy",
        "",
        "| Condition | Mean energy | Median energy | Stable rate |",
        "|---|---:|---:|---:|",
    ]
    for condition in ("gold", "missing", "contradiction", "duplicate", "distractor", "wrong"):
        lines.append(
            f"| {condition} | {overall['energy_mean'][condition]:.6f} | "
            f"{overall['energy_median'][condition]:.6f} | "
            f"{percent(overall['stable_rate'][condition])} |"
        )
    lines.extend(["", "## Results by question type", ""])
    for qtype, metrics in sorted(summary["by_type"].items()):
        lines.append(
            f"- **{qtype}** ({metrics['examples']}): missing {percent(metrics['missing_hop_detection_rate'])}, "
            f"contradiction {percent(metrics['contradiction_detection_rate'])}, "
            f"duplicate-safe {percent(metrics['duplicate_non_improvement_rate'])}, "
            f"distractor-safe {percent(metrics['distractor_non_improvement_rate'])}."
        )
    lines.extend(
        [
            "",
            "## Interpretation rule",
            "",
            "This benchmark does not measure answer exact match or retrieval recall. It isolates the actual `graphene::LyapunovCritic` and asks whether controlled evidence corruption raises its energy. The 2Wiki records provide gold supporting facts and structured evidence paths, but no reliable event-time field, so temporal consistency is held constant and remains unvalidated here.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--json-output", required=True)
    parser.add_argument("--markdown-output", required=True)
    args = parser.parse_args()

    with Path(args.input).open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise RuntimeError("benchmark result CSV is empty")

    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row["type"]].append(row)
    summary = {
        "benchmark": "2WikiMultiHopQA Lyapunov evidence-corruption diagnostic",
        "critic": "graphene::LyapunovCritic",
        "mode": "empirical",
        "temporal_coordinate_tested": False,
        "overall": group_metrics(rows),
        "by_type": {key: group_metrics(value) for key, value in grouped.items()},
        "initial_targets": {
            "missing_hop_detection_rate": 0.90,
            "contradiction_detection_rate": 0.90,
            "duplicate_non_improvement_rate": 0.98,
            "distractor_non_improvement_rate": 0.95,
            "repair_monotonic_rate": 0.90,
        },
    }
    overall = summary["overall"]
    summary["meets_initial_diagnostic_targets"] = all(
        overall[key] >= target for key, target in summary["initial_targets"].items()
    )

    Path(args.json_output).write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    Path(args.markdown_output).write_text(render_markdown(summary), encoding="utf-8")
    print(json.dumps(summary["overall"], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
