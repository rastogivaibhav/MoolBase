#!/usr/bin/env python3
"""Summarise the controlled dialectic intervention benchmark."""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

HARD_FAMILIES = {
    "missing_hop_2",
    "missing_hop_3",
    "missing_hop_4",
    "minority_path",
    "independent_candidate",
    "noise_trap",
}


def ratio(numerator: float, denominator: float) -> float:
    return 0.0 if denominator == 0 else numerator / denominator


def load_rows(path: Path) -> list[dict[str, Any]]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError(f"no benchmark rows found in {path}")
    integer_fields = {
        "seed",
        "initial_correct",
        "final_correct",
        "corrected",
        "degraded",
        "useful_new_information",
        "cycles",
        "total_visited",
        "first_correct_cycle",
        "oracle_cycle",
        "stop_regret_cycles",
        "final_primary",
        "final_roots",
        "final_paths",
    }
    for row in rows:
        for field in integer_fields:
            row[field] = int(row[field])
        row["final_confidence"] = float(row["final_confidence"])
    return rows


def aggregate(rows: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    policies: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(row["family"], row["policy"])].append(row)
        if row["family"] in HARD_FAMILIES:
            policies[row["policy"]].append(row)

    family_policy: dict[str, Any] = {}
    for (family, policy), values in sorted(grouped.items()):
        key = f"{family}:{policy}"
        family_policy[key] = {
            "runs": len(values),
            "initial_accuracy": ratio(sum(v["initial_correct"] for v in values), len(values)),
            "final_accuracy": ratio(sum(v["final_correct"] for v in values), len(values)),
            "mean_cycles": ratio(sum(v["cycles"] for v in values), len(values)),
            "mean_visited": ratio(sum(v["total_visited"] for v in values), len(values)),
            "mean_stop_regret_cycles": ratio(
                sum(v["stop_regret_cycles"] for v in values), len(values)
            ),
        }

    policy_summary: dict[str, Any] = {}
    for policy, values in sorted(policies.items()):
        policy_summary[policy] = {
            "runs": len(values),
            "final_accuracy": ratio(sum(v["final_correct"] for v in values), len(values)),
            "mean_cycles": ratio(sum(v["cycles"] for v in values), len(values)),
            "mean_visited": ratio(sum(v["total_visited"] for v in values), len(values)),
            "correction_rate": ratio(sum(v["corrected"] for v in values), len(values)),
            "harm_rate": ratio(sum(v["degraded"] for v in values), len(values)),
        }

    gates = {
        "frontier_aware_solves_all_hard_families": policy_summary[
            "targeted_frontier_aware"
        ]["final_accuracy"] == 1.0,
        "current_stop_exposes_deep_chain_failure": policy_summary[
            "targeted_current_stop"
        ]["final_accuracy"] < 1.0,
        "targeted_uses_less_search_than_broad": policy_summary[
            "targeted_frontier_aware"
        ]["mean_visited"] < policy_summary["broad_forced_3"]["mean_visited"],
        "broad_noise_trap_fails": family_policy[
            "noise_trap:broad_forced_3"
        ]["final_accuracy"] == 0.0,
        "frontier_aware_noise_trap_passes": family_policy[
            "noise_trap:targeted_frontier_aware"
        ]["final_accuracy"] == 1.0,
    }
    return {
        "schema_version": 1,
        "total_runs": len(rows),
        "hard_families": sorted(HARD_FAMILIES),
        "policy_summary": policy_summary,
        "family_policy": family_policy,
        "gates": gates,
        "all_gates_pass": all(gates.values()),
    }


def markdown(summary: dict[str, Any]) -> str:
    lines = [
        "# Dialectic intervention benchmark — frontier-aware rerun",
        "",
        f"Total deterministic executions: **{summary['total_runs']}**.",
        "",
        "## Hard-family aggregate",
        "",
        "| Policy | Final accuracy | Mean cycles | Mean visited states |",
        "|---|---:|---:|---:|",
    ]
    for policy, values in summary["policy_summary"].items():
        lines.append(
            f"| `{policy}` | {values['final_accuracy']:.1%} | "
            f"{values['mean_cycles']:.2f} | {values['mean_visited']:.2f} |"
        )
    lines += ["", "## Frozen diagnostic gates", ""]
    for gate, passed in summary["gates"].items():
        lines.append(f"- {'PASS' if passed else 'FAIL'} — `{gate}`")
    lines += [
        "",
        "## Claim boundary",
        "",
        "This controlled benchmark isolates cycle, escape and stopping behaviour. "
        "It does not establish semantic truth, public-dataset generalisation or "
        "end-to-end superiority over external agent systems.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path, required=True)
    parser.add_argument("--enforce", action="store_true")
    args = parser.parse_args()

    summary = aggregate(load_rows(args.input))
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    args.markdown_output.write_text(markdown(summary), encoding="utf-8")
    print(markdown(summary))
    return 1 if args.enforce and not summary["all_gates_pass"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
