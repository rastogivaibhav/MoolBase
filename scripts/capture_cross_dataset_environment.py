#!/usr/bin/env python3
"""Capture a reproducible environment and input manifest for the cross-dataset run."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def command_output(command: list[str]) -> str:
    try:
        completed = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        return completed.stdout.strip()
    except OSError as error:
        return f"unavailable: {error}"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--output", required=True)
    parser.add_argument("--mode", required=True, choices=("offline", "public", "all"))
    parser.add_argument("--compiler", default="")
    parser.add_argument("--input", action="append", default=[])
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    compiler = args.compiler or shutil.which("g++") or shutil.which("clang++") or ""
    source_override = os.getenv("GRAPHENEDB_SOURCE_COMMIT_OVERRIDE", "").strip()
    source_commit = source_override or command_output(["git", "-C", str(repo), "rev-parse", "HEAD"])
    dirty = "" if source_override else command_output(["git", "-C", str(repo), "status", "--porcelain"])

    inputs: dict[str, dict[str, object]] = {}
    for raw in args.input:
        path = Path(raw)
        if not path.is_absolute():
            path = repo / path
        record: dict[str, object] = {"path": str(path)}
        if path.is_file():
            record.update({"size": path.stat().st_size, "sha256": sha256(path)})
        else:
            record["missing"] = True
        inputs[str(path.relative_to(repo)) if path.is_relative_to(repo) else str(path)] = record

    payload = {
        "benchmark": "GrapheneDB cross-dataset epistemic structural suite",
        "claim_boundary": "Evidence-structure and governed-stability diagnostic; not QA exact match or semantic truth generation.",
        "mode": args.mode,
        "source_commit": source_commit,
        "source_commit_override_used": bool(source_override),
        "working_tree_dirty": bool(dirty),
        "working_tree_changes": dirty.splitlines(),
        "timestamp_utc": command_output([sys.executable, "-c", "import datetime; print(datetime.datetime.now(datetime.timezone.utc).isoformat())"]),
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": sys.version.replace("\n", " "),
        },
        "compiler": {
            "path": compiler,
            "version": command_output([compiler, "--version"]) if compiler else "unavailable",
        },
        "git": command_output(["git", "--version"]),
        "docker": command_output(["docker", "--version"]),
        "environment": {
            "BABI_PER_TASK": os.getenv("BABI_PER_TASK", "200"),
            "HOTPOT": os.getenv("HOTPOT", "500"),
            "FEVER_PER_LABEL": os.getenv("FEVER_PER_LABEL", "200"),
            "CROSS_DATASET_SEED": os.getenv("CROSS_DATASET_SEED", "20260729"),
        },
        "inputs": inputs,
    }
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
