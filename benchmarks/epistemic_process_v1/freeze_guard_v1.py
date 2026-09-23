#!/usr/bin/env python3
"""Pre-freeze integrity guard for Epistemic Process Evaluation v1.

This guard intentionally fails until a production-runtime runner exists and is
explicitly declared. It prevents the deterministic plumbing oracle in run_v1.py
from being mistaken for score-bearing system-under-test execution.

It does not change benchmark tasks, scoring semantics, or expected answers.
"""
from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ORACLE_RUNNER = ROOT / "run_v1.py"
PREREG = ROOT / "preregistration.json"

FORBIDDEN_SUT_HINTS = {
    "terminal_supported",
    "decisive",
}
FORBIDDEN_ORACLE_HELPERS = {
    "_choose",
    "_is_refuted",
    "_independent_support",
}


def names_used(path: Path) -> set[str]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    names: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Name):
            names.add(node.id)
        elif isinstance(node, ast.Attribute):
            names.add(node.attr)
        elif isinstance(node, ast.Constant) and isinstance(node.value, str):
            names.add(node.value)
    return names


def verify_production_runner(path: Path) -> list[str]:
    errors: list[str] = []
    if not path.exists():
        return [f"production runtime runner missing: {path}"]

    used = names_used(path)
    leaked = sorted(FORBIDDEN_SUT_HINTS & used)
    if leaked:
        errors.append(
            "production runner references scoring-only hints: " + ", ".join(leaked)
        )
    copied = sorted(FORBIDDEN_ORACLE_HELPERS & used)
    if copied:
        errors.append(
            "production runner references plumbing-oracle helpers: " + ", ".join(copied)
        )
    return errors


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--production-runner",
        default=str(ROOT / "run_production_v1.py"),
        help="candidate production-runtime runner",
    )
    ap.add_argument(
        "--require-freeze-ready",
        action="store_true",
        help="fail unless all production-runtime pre-freeze conditions are met",
    )
    args = ap.parse_args()

    prereg = json.loads(PREREG.read_text(encoding="utf-8"))
    errors: list[str] = []

    # Score-bearing mode must remain locked until a dedicated freeze change.
    if prereg.get("score_bearing_runs_allowed") is not False:
        errors.append("preregistration score-bearing lock is not false")

    # The plumbing runner is allowed to contain oracle logic, but must never be
    # accepted as the production system-under-test runner.
    oracle_used = names_used(ORACLE_RUNNER)
    if not (FORBIDDEN_ORACLE_HELPERS & oracle_used):
        errors.append("expected plumbing-oracle markers not found in run_v1.py")

    production = Path(args.production_runner)
    production_errors = verify_production_runner(production)

    report = {
        "protocol": "epistemic-process-v1",
        "score_bearing_locked": prereg.get("score_bearing_runs_allowed") is False,
        "plumbing_runner": str(ORACLE_RUNNER),
        "production_runner": str(production),
        "production_runner_present": production.exists(),
        "production_runner_errors": production_errors,
        "freeze_ready": not errors and not production_errors,
        "guard_errors": errors,
    }
    print(json.dumps(report, indent=2, sort_keys=True))

    if errors:
        raise SystemExit("freeze integrity guard failed")
    if args.require_freeze_ready and production_errors:
        raise SystemExit("freeze blocked: production-runtime execution is not ready")


if __name__ == "__main__":
    main()
