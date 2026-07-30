#!/usr/bin/env python3
"""Negative and positive contracts for the G0 preregistration manifest."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(*args: str, cwd: pathlib.Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(args),
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def git(repo: pathlib.Path, *args: str) -> str:
    completed = run("git", "-C", str(repo), *args)
    require(completed.returncode == 0, f"git {' '.join(args)} failed: {completed.stderr}")
    return completed.stdout.strip()


def initialize_repo(path: pathlib.Path, files: dict[str, str]) -> str:
    path.mkdir(parents=True)
    require(run("git", "init", "-q", str(path)).returncode == 0, "git init failed")
    git(path, "config", "user.email", "g0-test@example.invalid")
    git(path, "config", "user.name", "G0 Test")
    for name, content in files.items():
        target = path / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")
    git(path, "add", ".")
    git(path, "commit", "-q", "-m", "fixture")
    return git(path, "rev-parse", "HEAD")


def invoke(script: pathlib.Path, *args: str) -> subprocess.CompletedProcess[str]:
    return run(sys.executable, str(script), *args)


def identity_hash(value: object) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_deepmind_foundation_manifest.py SCRIPT")
    script = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="graphenedb-g0-manifest-") as temporary:
        root = pathlib.Path(temporary)
        source = root / "source"
        initialize_repo(
            source,
            {
                "deepmindtest.md": "# fixture benchmark\n",
                "bench/deepmind/generate_causal_suite.cpp": "// fixture generator\n",
                "tracked.txt": "pinned source\n",
            },
        )
        d1 = root / "d1"
        d1_commit = initialize_repo(d1, {"postmortem.md": "# pinned incident\n"})
        d0 = root / "d0.jsonl"
        with d0.open("w", encoding="utf-8", newline="\n") as output:
            for index in range(20_000):
                output.write(f'{{"query_id":"q{index}"}}\n')
        profiles = {}
        for name in ("hardware", "tools", "versions", "retrieval"):
            path = root / f"{name}.json"
            path.write_text(json.dumps({"profile": name}, sort_keys=True) + "\n", encoding="utf-8")
            profiles[name] = path
        manifest = root / "manifest.json"
        common = [
            "--repo",
            str(source),
            "--output",
            str(manifest),
            "--run-id",
            "g0-fixture-001",
            "--d0-dataset",
            str(d0),
            "--d1-corpus",
            str(d1),
            "--d1-commit",
            d1_commit,
            "--image-ref",
            "graphenedb:test",
            "--image-digest",
            "sha256:" + "a" * 64,
            "--hardware-profile",
            str(profiles["hardware"]),
            "--tool-permissions",
            str(profiles["tools"]),
            "--external-versions",
            str(profiles["versions"]),
            "--retrieval-config",
            str(profiles["retrieval"]),
            "--policy-version",
            "dialectic-empirical-d0-v1",
            "--seed",
            "424242",
        ]
        created = invoke(script, "create", *common)
        require(created.returncode == 0, f"valid creation failed:\n{created.stderr}")
        valid = invoke(script, "validate", "--manifest", str(manifest), "--check-live")
        require(valid.returncode == 0, f"valid manifest rejected:\n{valid.stderr}")

        original = manifest.read_bytes()
        missing_hash = json.loads(original)
        del missing_hash["immutable"]["benchmark"]["source_sha256"]
        missing_hash["identity_lock_sha256"] = identity_hash(missing_hash["immutable"])
        manifest.write_text(json.dumps(missing_hash), encoding="utf-8")
        rejected = invoke(script, "validate", "--manifest", str(manifest))
        require(rejected.returncode != 0, "manifest with a missing required hash passed")

        manifest.write_bytes(original)
        null_start = json.loads(original)
        null_start["run_started_at_utc"] = None
        manifest.write_text(json.dumps(null_start), encoding="utf-8")
        rejected = invoke(script, "validate", "--manifest", str(manifest))
        require(rejected.returncode != 0, "manifest without a pre-run timestamp passed")

        manifest.write_bytes(original)
        (source / "untracked.txt").write_text("dirty\n", encoding="utf-8")
        dirty_output = root / "dirty-manifest.json"
        dirty_args = list(common)
        dirty_args[dirty_args.index(str(manifest))] = str(dirty_output)
        rejected = invoke(script, "create", *dirty_args)
        require(rejected.returncode != 0, "dirty checkout produced a G0 manifest")
        (source / "untracked.txt").unlink()

        evidence = root / "ctest.log"
        evidence.write_text("100% tests passed\n", encoding="utf-8")
        result_args = [
            "--result",
            "ctest=true",
            "--result",
            "openapi_routes=true",
            "--result",
            "package_consumer=true",
            "--result",
            "docker_non_root=true",
            "--result",
            "clean_checkout_rerun=true",
            "--result",
            "d0_g2=true",
            "--result",
            "git_diff_check=true",
        ]
        finalized = invoke(
            script,
            "finalize",
            "--manifest",
            str(manifest),
            *result_args,
            "--artifact",
            f"ctest={evidence}",
        )
        require(finalized.returncode == 0, f"valid finalization failed:\n{finalized.stderr}")
        complete = invoke(
            script,
            "validate",
            "--manifest",
            str(manifest),
            "--check-live",
            "--require-complete",
        )
        require(complete.returncode == 0, f"completed manifest rejected:\n{complete.stderr}")

        tampered = json.loads(manifest.read_text(encoding="utf-8"))
        tampered["immutable"]["datasets"]["D0"]["seed"] += 1
        manifest.write_text(json.dumps(tampered), encoding="utf-8")
        rejected = invoke(script, "validate", "--manifest", str(manifest))
        require(rejected.returncode != 0, "post-run identity tampering passed")

    print("deepmind_g0_manifest_contract_passed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
