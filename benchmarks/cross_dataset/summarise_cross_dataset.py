#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean
from typing import Any, Iterable

EPS = 1e-9


def rate(values: Iterable[bool]) -> float:
    values = list(values)
    return sum(values) / len(values) if values else math.nan


def f(row: dict[str, str], key: str) -> float:
    return float(row[key])


def b(row: dict[str, str], key: str) -> bool:
    return row[key] == "1"


def support_metrics(rows: list[dict[str, str]]) -> dict[str, Any]:
    return {
        "examples": len(rows),
        "missing_ordering": rate(f(r, "missing_energy") > f(r, "gold_energy") + EPS for r in rows),
        "contradiction_ordering": rate(f(r, "contradiction_energy") > f(r, "gold_energy") + EPS for r in rows),
        "duplicate_non_improvement": rate(f(r, "duplicate_energy") >= f(r, "gold_energy") - EPS for r in rows),
        "distractor_non_improvement": rate(f(r, "distractor_energy") >= f(r, "gold_energy") - EPS for r in rows),
        "duplicate_independence_safe": rate(int(r["duplicate_independent"]) == int(r["gold_independent"]) for r in rows),
        "independent_support_detected": rate(int(r["independent_independent"]) > int(r["gold_independent"]) for r in rows),
        "independent_score_non_worsening": rate(f(r, "independent_score") + EPS >= f(r, "gold_score") for r in rows),
        "contradiction_blocks": rate(b(r, "contradiction_blocks") for r in rows),
        "missing_not_stable": rate(not b(r, "missing_stable") for r in rows),
        "distractor_not_stable": rate(not b(r, "distractor_stable") for r in rows),
        "energy_mean": {
            key: mean(f(r, key) for r in rows)
            for key in (
                "gold_energy", "missing_energy", "duplicate_energy",
                "distractor_energy", "contradiction_energy", "independent_energy"
            )
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--json-output", required=True)
    parser.add_argument("--markdown-output", required=True)
    parser.add_argument("--enforce", action="store_true")
    args = parser.parse_args()

    with Path(args.input).open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    supports = [r for r in rows if r["category"] == "SUPPORT"]
    refutes = [r for r in rows if r["category"] == "REFUTE"]
    nei = [r for r in rows if r["category"] == "NEI"]
    temporal = [r for r in supports if r["temporal"] == "1"]
    by_dataset: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in supports:
        by_dataset[row["dataset"]].append(row)

    summary: dict[str, Any] = {
        "benchmark": "GrapheneDB cross-dataset epistemic structural suite",
        "examples": len(rows),
        "support": support_metrics(supports),
        "refute_blocks": rate(b(r, "refute_blocks") for r in refutes),
        "nei_not_stable": rate(b(r, "nei_not_stable") for r in nei),
        "nei_repair_required": rate(b(r, "nei_repair_required") for r in nei),
        "temporal_mismatch_ordering": rate(
            f(r, "temporal_invalid_energy") > f(r, "gold_energy") + EPS for r in temporal
        ),
        "temporal_mismatch_not_stable": rate(
            not b(r, "temporal_invalid_stable") for r in temporal
        ),
        "support_by_dataset": {name: support_metrics(items) for name, items in sorted(by_dataset.items())},
        "targets": {
            "missing_ordering": 0.99,
            "contradiction_ordering": 0.99,
            "duplicate_non_improvement": 1.0,
            "distractor_non_improvement": 1.0,
            "duplicate_independence_safe": 1.0,
            "independent_support_detected": 0.99,
            "independent_score_non_worsening": 0.99,
            "contradiction_blocks": 1.0,
            "refute_blocks": 1.0,
            "nei_not_stable": 1.0,
            "nei_repair_required": 1.0,
            "temporal_mismatch_ordering": 0.99,
            "temporal_mismatch_not_stable": 1.0,
        },
        "claim_boundary": "Tests evidence structure and governed stability, not QA exact match or semantic truth generation.",
    }
    actual = {
        **{key: summary["support"][key] for key in summary["targets"] if key in summary["support"]},
        "refute_blocks": summary["refute_blocks"],
        "nei_not_stable": summary["nei_not_stable"],
        "nei_repair_required": summary["nei_repair_required"],
        "temporal_mismatch_ordering": summary["temporal_mismatch_ordering"],
        "temporal_mismatch_not_stable": summary["temporal_mismatch_not_stable"],
    }
    summary["meets_targets"] = all(
        actual[key] + EPS >= target for key, target in summary["targets"].items()
    )

    def pct(value: float) -> str:
        return "n/a" if math.isnan(value) else f"{100.0 * value:.1f}%"

    lines = [
        "# Cross-dataset epistemic benchmark",
        "",
        f"Examples: **{len(rows)}**",
        "",
        "| Diagnostic | Result | Target |",
        "|---|---:|---:|",
    ]
    for key, target in summary["targets"].items():
        lines.append(f"| {key.replace('_', ' ')} | {pct(actual[key])} | {pct(target)} |")
    lines.extend(["", f"Overall gate: **{'PASS' if summary['meets_targets'] else 'FAIL'}**", ""])
    for name, metrics in summary["support_by_dataset"].items():
        lines.append(
            f"- **{name}** ({metrics['examples']} support examples): "
            f"missing {pct(metrics['missing_ordering'])}, contradiction {pct(metrics['contradiction_ordering'])}, "
            f"duplicate-safe {pct(metrics['duplicate_non_improvement'])}, distractor-safe {pct(metrics['distractor_non_improvement'])}."
        )
    lines.extend(["", "This suite does not measure answer exact match. It evaluates the actual FiberBundle v2 and Lyapunov critic against benchmark-derived evidence structures and labels.", ""])

    Path(args.json_output).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    Path(args.markdown_output).write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    if args.enforce and not summary["meets_targets"]:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
