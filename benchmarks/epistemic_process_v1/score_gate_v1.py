#!/usr/bin/env python3
"""Single score-bearing authorization gate for Epistemic Process v1.

Score-bearing execution is authorized only by an immutable FROZEN manifest whose
listed file hashes match the checkout exactly. No task field, environment
variable, or command-line mode can independently unlock scoring.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from typing import Any, Dict, Mapping

ROOT = Path(__file__).resolve().parents[2]
REQUIRED_MANIFEST_SCHEMA = "epistemic-process-score-freeze-v1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_head(root: Path = ROOT) -> str | None:
    try:
        proc = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return None
    if proc.returncode != 0:
        return None
    return proc.stdout.strip() or None


def is_ancestor(
    candidate: str,
    *,
    root: Path = ROOT,
) -> bool | None:
    try:
        proc = subprocess.run(
            ["git", "merge-base", "--is-ancestor", candidate, "HEAD"],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return None
    if proc.returncode == 0:
        return True
    if proc.returncode == 1:
        return False
    return None


def changes_since_candidate(
    candidate: str,
    *,
    root: Path = ROOT,
) -> list[str] | None:
    try:
        proc = subprocess.run(
            ["git", "diff", "--name-only", f"{candidate}..HEAD"],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return None
    if proc.returncode != 0:
        return None
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def verify_score_gate(
    manifest_path: Path,
    *,
    root: Path = ROOT,
) -> Dict[str, Any]:
    errors: list[str] = []
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return {
            "valid": False,
            "errors": [f"cannot load freeze manifest: {exc}"],
            "manifest": None,
            "head": git_head(root),
        }

    if manifest.get("schema") != REQUIRED_MANIFEST_SCHEMA:
        errors.append("unsupported freeze manifest schema")
    if manifest.get("status") != "FROZEN":
        errors.append("freeze manifest status is not FROZEN")
    if manifest.get("score_bearing_authorized") is not True:
        errors.append("freeze manifest does not authorize score-bearing execution")
    if not manifest.get("experiment_id"):
        errors.append("freeze manifest has no experiment_id")

    frozen_files = manifest.get("frozen_files")
    if not isinstance(frozen_files, Mapping) or not frozen_files:
        errors.append("freeze manifest has no frozen_files")
        frozen_files = {}

    observed: Dict[str, str] = {}
    for raw_path, expected in sorted(frozen_files.items()):
        relative = Path(str(raw_path))
        if relative.is_absolute() or ".." in relative.parts:
            errors.append(f"unsafe frozen file path: {raw_path}")
            continue
        path = root / relative
        if not path.is_file():
            errors.append(f"frozen file missing: {raw_path}")
            continue
        actual = sha256_file(path)
        observed[str(raw_path)] = actual
        if actual != str(expected):
            errors.append(
                f"frozen file hash mismatch: {raw_path}: "
                f"expected {expected}, observed {actual}"
            )

    required_profiles = {"B0", "G0", "G1", "G2"}
    profiles = manifest.get("configuration_profiles")
    if not isinstance(profiles, Mapping) or set(profiles) != required_profiles:
        errors.append("freeze manifest must define exactly B0/G0/G1/G2 profiles")

    candidate = str(manifest.get("benchmark_candidate_commit") or "")
    allowed_changes = set(manifest.get("allowed_changes_after_candidate") or [])
    observed_changes = None
    if not candidate:
        errors.append("freeze manifest has no benchmark_candidate_commit")
    if candidate and git_head(root):
        ancestor = is_ancestor(candidate, root=root)
        if ancestor is False:
            errors.append("benchmark candidate commit is not an ancestor of HEAD")
        elif ancestor is None:
            errors.append("cannot verify benchmark candidate ancestry")
        observed_changes = changes_since_candidate(candidate, root=root)
        if observed_changes is None:
            errors.append(
                "cannot verify repository changes since benchmark candidate commit"
            )
        else:
            unexpected = sorted(set(observed_changes) - allowed_changes)
            if unexpected:
                errors.append(
                    "unexpected changes after benchmark candidate: "
                    + ", ".join(unexpected)
                )

    policies = manifest.get("policies")
    required_policies = {
        "seed_policy",
        "repetition_policy",
        "timeout_policy",
        "runtime_failure_policy",
        "adapter_failure_policy",
        "missing_receipt_policy",
        "abstention_policy",
    }
    if not isinstance(policies, Mapping):
        errors.append("freeze manifest has no policies")
    else:
        missing_policies = sorted(required_policies - set(policies))
        if missing_policies:
            errors.append(
                "freeze manifest missing policies: " + ", ".join(missing_policies)
            )

    return {
        "valid": not errors,
        "errors": errors,
        "experiment_id": manifest.get("experiment_id"),
        "benchmark_candidate_commit": manifest.get("benchmark_candidate_commit"),
        "production_architecture_commit": manifest.get(
            "production_architecture_commit"
        ),
        "head": git_head(root),
        "observed_hashes": observed,
        "observed_changes_after_candidate": observed_changes,
        "manifest": manifest,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = verify_score_gate(Path(args.manifest))
    print(json.dumps(result, indent=2, sort_keys=True))
    if not result["valid"]:
        raise SystemExit("score gate blocked")


if __name__ == "__main__":
    main()
