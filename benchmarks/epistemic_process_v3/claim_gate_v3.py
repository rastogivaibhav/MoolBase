#!/usr/bin/env python3
"""Generic pre-score claim-gate evaluator for EP-PROCESS-V3.

This module evaluates only preregistered synthetic or future frozen metric
summaries. It does not execute MoolBase, load score tasks, or authorize scoring.
"""
from __future__ import annotations

import json
import operator
from pathlib import Path
from typing import Any, Mapping

OPS = {
    ">=": operator.ge,
    "<=": operator.le,
    ">": operator.gt,
    "<": operator.lt,
    "==": operator.eq,
}


def _value(summary: Mapping[str, Any], name: str) -> Any:
    if name not in summary:
        raise KeyError(f"missing claim metric: {name}")
    return summary[name]


def evaluate_claims(
    summary: Mapping[str, Any],
    gates: Mapping[str, Any],
    eligibility: Mapping[str, bool] | None = None,
) -> dict[str, Any]:
    eligibility = eligibility or {}
    results: dict[str, Any] = {}

    for claim_id, spec in gates["claims"].items():
        if eligibility.get(claim_id, True) is False:
            results[claim_id] = {
                "status": "INELIGIBLE_DESIGN",
                "checks": {},
            }
            continue

        prerequisite_results = {}
        prereq_ok = True
        for prerequisite in spec.get("prerequisites", []):
            earned = (
                results.get(prerequisite, {}).get("status") == "EARNED"
            )
            prerequisite_results[prerequisite] = earned
            prereq_ok = prereq_ok and earned

        checks = {}
        for metric, op_name, threshold in spec.get("all", []):
            if op_name not in OPS:
                raise ValueError(f"unsupported operator: {op_name}")
            actual = _value(summary, metric)
            passed = (
                actual is not None
                and OPS[op_name](float(actual), float(threshold))
            )
            checks[metric] = {
                "actual": actual,
                "operator": op_name,
                "threshold": threshold,
                "passed": bool(passed),
            }

        status = (
            "EARNED"
            if prereq_ok and all(row["passed"] for row in checks.values())
            else "NOT_EARNED"
        )
        results[claim_id] = {
            "status": status,
            "checks": checks,
            "prerequisites": prerequisite_results,
        }

    return {
        "schema": "epistemic-process-v3-claim-gate-result-v1",
        "score_bearing_authorized": False,
        "claims": results,
    }


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", required=True)
    parser.add_argument("--gates", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    summary = json.loads(Path(args.summary).read_text(encoding="utf-8"))
    gates = json.loads(Path(args.gates).read_text(encoding="utf-8"))
    result = evaluate_claims(summary, gates)
    Path(args.out).write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
