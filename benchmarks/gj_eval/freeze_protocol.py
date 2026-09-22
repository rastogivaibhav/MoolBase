#!/usr/bin/env python3
"""Emit a reproducibility receipt for the frozen GJ-Eval protocol."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import subprocess


HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]

FILES = [
    "docs/benchmarks/GJ_EVAL_V1_SPEC.md",
    "benchmarks/gj_eval/manifest.v1.json",
    "benchmarks/gj_eval/ablation_status.v1.json",
    "benchmarks/gj_eval/schemas/world.v1.schema.json",
    "benchmarks/gj_eval/schemas/prediction.v1.schema.json",
    "benchmarks/gj_eval/generate_worldshift.py",
    "benchmarks/gj_eval/run_command_adapter.py",
    "benchmarks/gj_eval/score.py",
    "benchmarks/gj_eval/compare.py",
    "benchmarks/gj_eval/adapters/jev_http.py",
    "benchmarks/gj_eval/adapters/graphene_deterministic.py",
    "benchmarks/gj_eval/adapters/graphene_dialectic.py",
    "benchmarks/gj_eval/adapters/graphene_hypokosh.py",
    "benchmarks/gj_eval/adapters/graphene_http.py",
]


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output")
    args = parser.parse_args()

    try:
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
    except Exception:
        commit = None

    secret_seed = os.environ.get("GJ_EVAL_TEST_SEED")
    receipt = {
        "schema_version": 1,
        "benchmark": "GJ-Eval",
        "protocol_version": "1.0",
        "created_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "git_commit": commit,
        "files": {
            name: sha256(ROOT / name)
            for name in FILES
        },
        "hidden_test_seed_sha256": (
            hashlib.sha256(secret_seed.encode()).hexdigest()
            if secret_seed is not None
            else None
        ),
    }
    encoded = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    if args.output:
        pathlib.Path(args.output).write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
