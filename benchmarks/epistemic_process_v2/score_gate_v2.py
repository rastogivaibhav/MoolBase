#!/usr/bin/env python3
"""Immutable authorization gate for EP-PROCESS-V2-SCORE-001.

The gate is intentionally independent of score computation. It verifies that
the score-bearing run is executing on master, that the pre-freeze candidate
commit/tree are exact, and that the only content change after that candidate is
the immutable score-freeze manifest itself.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path
from typing import Any, Mapping

SCHEMA = "epistemic-process-v2-score-freeze-v1"
EXPERIMENT = "EP-PROCESS-V2-SCORE-001"
MANIFEST_PATH = "benchmarks/epistemic_process_v2/score_freeze_v2.json"
PROFILES = ["C0", "G0E", "C1", "G1", "G2"]


class ScoreAuthorizationError(RuntimeError):
    pass


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise ScoreAuthorizationError(
            f"git {' '.join(args)} failed: {result.stderr.strip()}"
        )
    return result.stdout.strip()


def validate_manifest_contract(manifest: Mapping[str, Any]) -> None:
    required = {
        "schema": SCHEMA,
        "status": "FROZEN",
        "experiment_id": EXPERIMENT,
        "score_bearing_authorized": True,
        "score_repetitions": 1,
        "automatic_retry": False,
        "imputation": False,
        "task_episodes": 270,
        "task_families": 9,
        "episodes_per_family": 30,
        "runtime_seed": 20261005,
        "bootstrap_seed": 20261006,
        "bootstrap_resamples": 10000,
    }
    for key, expected in required.items():
        if manifest.get(key) != expected:
            raise ScoreAuthorizationError(
                f"manifest {key!r} expected {expected!r}, "
                f"got {manifest.get(key)!r}"
            )

    if manifest.get("profiles") != PROFILES:
        raise ScoreAuthorizationError(
            f"profiles must be exactly {PROFILES!r}"
        )
    if manifest.get("allowed_changes_after_candidate") != [MANIFEST_PATH]:
        raise ScoreAuthorizationError(
            "only the score-freeze manifest may change after the candidate"
        )

    for field in (
        "candidate_commit",
        "candidate_tree",
        "evaluator_freeze_commit",
        "evaluator_freeze_manifest_sha256",
    ):
        value = str(manifest.get(field) or "")
        if not value:
            raise ScoreAuthorizationError(f"manifest missing {field}")

    policies = manifest.get("policies") or {}
    if policies.get("candidate_outcomes_previously_opened") is not False:
        raise ScoreAuthorizationError(
            "manifest must attest candidate outcomes were not opened pre-freeze"
        )
    if policies.get("candidate_oracle_join_previously_performed") is not False:
        raise ScoreAuthorizationError(
            "manifest must attest no pre-freeze candidate oracle join"
        )
    if policies.get("post_freeze_tuning_allowed") is not False:
        raise ScoreAuthorizationError("post-freeze tuning must be forbidden")
    if policies.get("failed_episode_imputation") is not False:
        raise ScoreAuthorizationError("failed-episode imputation must be false")
    if policies.get("score_run_retry") is not False:
        raise ScoreAuthorizationError("score-run retry must be false")


def validate_git_boundary(
    manifest: Mapping[str, Any],
    repo: Path,
) -> dict[str, Any]:
    candidate = str(manifest["candidate_commit"])
    expected_tree = str(manifest["candidate_tree"])
    head = git(repo, "rev-parse", "HEAD")
    head_tree = git(repo, "rev-parse", "HEAD^{tree}")
    candidate_tree = git(repo, "rev-parse", f"{candidate}^{{tree}}")
    if candidate_tree != expected_tree:
        raise ScoreAuthorizationError(
            f"candidate tree mismatch expected={expected_tree} "
            f"actual={candidate_tree}"
        )

    merge_base = git(repo, "merge-base", candidate, head)
    if merge_base != candidate:
        raise ScoreAuthorizationError(
            "candidate commit is not an ancestor of scoring HEAD"
        )

    changed = [
        line.strip()
        for line in git(repo, "diff", "--name-only", f"{candidate}..HEAD").splitlines()
        if line.strip()
    ]
    if changed != [MANIFEST_PATH]:
        raise ScoreAuthorizationError(
            f"post-candidate changed paths must be exactly "
            f"[{MANIFEST_PATH!r}], got {changed!r}"
        )

    commit_count = int(git(repo, "rev-list", "--count", f"{candidate}..HEAD"))
    if commit_count != 1:
        raise ScoreAuthorizationError(
            f"expected exactly one post-candidate commit, got {commit_count}"
        )

    manifest_blob = git(repo, "ls-tree", "-r", "--name-only", "HEAD", "--", MANIFEST_PATH)
    if manifest_blob != MANIFEST_PATH:
        raise ScoreAuthorizationError("freeze manifest is not tracked at HEAD")

    return {
        "candidate_commit": candidate,
        "candidate_tree": candidate_tree,
        "head_commit": head,
        "head_tree": head_tree,
        "post_candidate_commit_count": commit_count,
        "changed_paths": changed,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--manifest",
        default=MANIFEST_PATH,
    )
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()

    repo = Path(args.repo_root).resolve()
    manifest_path = repo / args.manifest
    if not manifest_path.exists():
        raise SystemExit(
            f"score authorization denied: missing {args.manifest}"
        )
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    try:
        validate_manifest_contract(manifest)
        boundary = validate_git_boundary(manifest, repo)
        github_ref = os.environ.get("GITHUB_REF")
        github_sha = os.environ.get("GITHUB_SHA")
        if github_ref is not None and github_ref != "refs/heads/master":
            raise ScoreAuthorizationError(
                f"score run must execute on refs/heads/master, got {github_ref}"
            )
        if github_sha is not None and github_sha != boundary["head_commit"]:
            raise ScoreAuthorizationError(
                "GITHUB_SHA does not equal checked-out HEAD"
            )
    except ScoreAuthorizationError as exc:
        raise SystemExit(f"score authorization denied: {exc}") from exc

    print(json.dumps({
        "score_authorization": "passed",
        "experiment_id": EXPERIMENT,
        "score_bearing_authorized": True,
        **boundary,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
