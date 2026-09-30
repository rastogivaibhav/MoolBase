#!/usr/bin/env python3
"""Integrity guard for Epistemic Process Evaluation v1.

The production runner/oracle boundary and the score-bearing authorization are
separate. Score mode has exactly one executable authorization mechanism:
score_gate_v1.py validating a FROZEN hash manifest. Legacy preregistration/task
lock fields are informational and cannot independently unlock scoring.
"""
from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path

from score_gate_v1 import verify_score_gate

ROOT = Path(__file__).resolve().parent
ORACLE_RUNNER = ROOT / "run_v1.py"
PREREG = ROOT / "preregistration.json"
DEFAULT_FREEZE_MANIFEST = ROOT / "score_freeze_v1.json"

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
            "production runner references scoring-only hints: "
            + ", ".join(leaked)
        )
    copied = sorted(FORBIDDEN_ORACLE_HELPERS & used)
    if copied:
        errors.append(
            "production runner references plumbing-oracle helpers: "
            + ", ".join(copied)
        )
    return errors


def build_report(
    production_runner: Path,
    freeze_manifest: Path,
) -> dict:
    prereg = json.loads(PREREG.read_text(encoding="utf-8"))
    errors: list[str] = []

    oracle_used = names_used(ORACLE_RUNNER)
    if not (FORBIDDEN_ORACLE_HELPERS & oracle_used):
        errors.append("expected plumbing-oracle markers not found in run_v1.py")

    production_errors = verify_production_runner(production_runner)
    freeze_present = freeze_manifest.exists()
    if freeze_present:
        score_gate = verify_score_gate(freeze_manifest)
    else:
        score_gate = {
            "valid": False,
            "errors": ["freeze manifest not present"],
            "experiment_id": None,
        }

    return {
        "protocol": "epistemic-process-v1",
        "legacy_preregistration_score_bearing_runs_allowed": prereg.get(
            "score_bearing_runs_allowed"
        ),
        "plumbing_runner": str(ORACLE_RUNNER),
        "production_runner": str(production_runner),
        "production_runner_present": production_runner.exists(),
        "production_runner_errors": production_errors,
        "freeze_manifest": str(freeze_manifest),
        "freeze_manifest_present": freeze_present,
        "score_gate_valid": bool(score_gate.get("valid")),
        "score_gate_errors": list(score_gate.get("errors") or []),
        "experiment_id": score_gate.get("experiment_id"),
        "score_bearing_locked": not bool(score_gate.get("valid")),
        "freeze_ready": not errors and not production_errors,
        "guard_errors": errors,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--production-runner",
        default=str(ROOT / "run_production_v1.py"),
    )
    parser.add_argument(
        "--freeze-manifest",
        default=str(DEFAULT_FREEZE_MANIFEST),
    )
    parser.add_argument(
        "--require-freeze-ready",
        action="store_true",
        help="fail unless production runtime/oracle boundary is freeze-ready",
    )
    parser.add_argument(
        "--require-score-gate",
        action="store_true",
        help="fail unless an exact FROZEN score manifest authorizes scoring",
    )
    args = parser.parse_args()

    report = build_report(
        Path(args.production_runner),
        Path(args.freeze_manifest),
    )
    print(json.dumps(report, indent=2, sort_keys=True))

    if report["guard_errors"]:
        raise SystemExit("freeze integrity guard failed")
    if args.require_freeze_ready and (
        not report["freeze_ready"] or report["production_runner_errors"]
    ):
        raise SystemExit("freeze blocked: production-runtime execution is not ready")
    if args.require_score_gate and not report["score_gate_valid"]:
        raise SystemExit("score gate blocked")


if __name__ == "__main__":
    main()
