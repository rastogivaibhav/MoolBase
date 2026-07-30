#!/usr/bin/env python3
"""Create and validate a fail-closed G0 foundation-run manifest.

The manifest is the first evaluation artifact. Its immutable identity binds a
clean source checkout and every prepared input before any gate command runs.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
from typing import Any, Iterable


SCHEMA = "graphenedb-deepmind-foundation-manifest-v1"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
IMAGE_DIGEST_RE = re.compile(r"^sha256:([0-9a-f]{64})$")
REQUIRED_RESULTS = {
    "ctest",
    "openapi_routes",
    "package_consumer",
    "docker_non_root",
    "clean_checkout_rerun",
    "d0_g2",
    "git_diff_check",
}


class ManifestError(RuntimeError):
    pass


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_hash(value: Any) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return sha256_bytes(encoded.encode("utf-8"))


def run_git(repo: pathlib.Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise ManifestError(f"git {' '.join(args)} failed: {completed.stderr.strip()}")
    return completed.stdout


def clean_status(repo: pathlib.Path) -> tuple[bool, str]:
    status = run_git(repo, "status", "--porcelain=v1", "--untracked-files=all")
    return not bool(status.strip()), sha256_bytes(status.encode("utf-8"))


def tracked_tree_hash(repo: pathlib.Path) -> tuple[str, int]:
    raw = subprocess.run(
        ["git", "-C", str(repo), "ls-files", "-z"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if raw.returncode != 0:
        raise ManifestError(f"git ls-files failed: {raw.stderr.decode(errors='replace')}")
    paths = [entry for entry in raw.stdout.split(b"\0") if entry]
    digest = hashlib.sha256()
    for encoded in sorted(paths):
        relative = encoded.decode("utf-8", errors="surrogateescape")
        candidate = repo / relative
        if not candidate.is_file():
            raise ManifestError(f"tracked source file is missing: {relative}")
        digest.update(encoded)
        digest.update(b"\0")
        digest.update(bytes.fromhex(sha256_file(candidate)))
    return digest.hexdigest(), len(paths)


def directory_tree_hash(root: pathlib.Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    count = 0
    for path in sorted(
        (
            candidate
            for candidate in root.rglob("*")
            if candidate.is_file() and ".git" not in candidate.relative_to(root).parts
        ),
        key=lambda candidate: candidate.relative_to(root).as_posix(),
    ):
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(bytes.fromhex(sha256_file(path)))
        count += 1
    return digest.hexdigest(), count


def jsonl_count(path: pathlib.Path) -> int:
    count = 0
    with path.open("rb") as source:
        for line in source:
            if line.strip():
                count += 1
    return count


def checked_file(value: str, name: str) -> pathlib.Path:
    path = pathlib.Path(value).resolve()
    if not path.is_file():
        raise ManifestError(f"{name} is not a file: {path}")
    return path


def checked_directory(value: str, name: str) -> pathlib.Path:
    path = pathlib.Path(value).resolve()
    if not path.is_dir():
        raise ManifestError(f"{name} is not a directory: {path}")
    return path


def parse_utc(value: str, name: str) -> None:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ManifestError(f"{name} must be an RFC3339 UTC timestamp")
    try:
        dt.datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise ManifestError(f"{name} is invalid: {exc}") from exc


def require_sha(value: Any, name: str) -> None:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise ManifestError(f"{name} must be a lowercase SHA-256")


def load_json(path: pathlib.Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError(f"cannot read manifest {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ManifestError("manifest root must be an object")
    return value


def write_json_atomic(path: pathlib.Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        json.dump(value, output, indent=2, sort_keys=True)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())
    os.replace(temporary, path)


def none_component() -> dict[str, str]:
    return {"id": "none", "sha256": sha256_bytes(b"none")}


def create_manifest(args: argparse.Namespace) -> int:
    repo = checked_directory(args.repo, "repository")
    output = pathlib.Path(args.output).resolve()
    try:
        output.relative_to(repo)
    except ValueError:
        pass
    else:
        raise ManifestError("manifest output must be outside the source checkout")

    clean, status_hash = clean_status(repo)
    if not clean:
        raise ManifestError("G0 creation requires a clean checkout, including no untracked files")
    commit = run_git(repo, "rev-parse", "HEAD").strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ManifestError("source commit is not a full SHA-1")
    branch = run_git(repo, "rev-parse", "--abbrev-ref", "HEAD").strip()
    source_tree_sha, source_file_count = tracked_tree_hash(repo)

    d0_dataset = checked_file(args.d0_dataset, "D0 dataset")
    d0_records = jsonl_count(d0_dataset)
    if d0_records < 20_000:
        raise ManifestError(f"D0 dataset has {d0_records} records; formal minimum is 20000")
    d1_corpus = checked_directory(args.d1_corpus, "D1 corpus")
    d1_head = run_git(d1_corpus, "rev-parse", "HEAD").strip()
    if d1_head != args.d1_commit:
        raise ManifestError(f"D1 checkout is {d1_head}, expected {args.d1_commit}")
    if run_git(d1_corpus, "status", "--porcelain=v1", "--untracked-files=all").strip():
        raise ManifestError("D1 corpus checkout is dirty")
    d1_tree_sha, d1_file_count = directory_tree_hash(d1_corpus)

    benchmark = repo / "deepmindtest.md"
    generator = repo / "bench" / "deepmind" / "generate_causal_suite.cpp"
    for required in (benchmark, generator):
        if not required.is_file():
            raise ManifestError(f"required benchmark source is missing: {required}")

    profiles = {
        "hardware": checked_file(args.hardware_profile, "hardware profile"),
        "tool_permissions": checked_file(args.tool_permissions, "tool-permissions profile"),
        "external_versions": checked_file(args.external_versions, "external-versions profile"),
        "retrieval_parameters": checked_file(args.retrieval_config, "retrieval configuration"),
    }
    image_match = IMAGE_DIGEST_RE.fullmatch(args.image_digest)
    if not image_match:
        raise ManifestError("image digest must be sha256:<64 lowercase hex>")

    immutable: dict[str, Any] = {
        "benchmark": {
            "id": "GDB-DH-AM-1",
            "version": "1.0-draft",
            "source_sha256": sha256_file(benchmark),
            "generator_sha256": sha256_file(generator),
        },
        "run_id": args.run_id,
        "source": {
            "commit": commit,
            "branch": branch,
            "clean_checkout": True,
            "git_status_sha256": status_hash,
            "tracked_tree_sha256": source_tree_sha,
            "tracked_file_count": source_file_count,
        },
        "formats": {"storage": 2, "extraction": 1},
        "container": {
            "image_ref": args.image_ref,
            "digest": args.image_digest,
            "digest_sha256": image_match.group(1),
            "documented_uid": 10001,
        },
        "datasets": {
            "D0": {
                "sha256": sha256_file(d0_dataset),
                "records": d0_records,
                "seed": args.seed,
            },
            "D1": {
                "commit": args.d1_commit,
                "tree_sha256": d1_tree_sha,
                "file_count": d1_file_count,
            },
        },
        "models": {
            "embedding": {
                "id": "deepmind-d0-deterministic-vector-for-v1",
                "sha256": sha256_file(generator),
                "dimension": 16,
            },
            "generation": none_component(),
        },
        "prompts": none_component(),
        "retrieval": {
            "sha256": sha256_file(profiles["retrieval_parameters"]),
            "policy_version": args.policy_version,
        },
        "profiles": {
            key: {"sha256": sha256_file(path)} for key, path in profiles.items() if key != "retrieval_parameters"
        },
    }
    manifest: dict[str, Any] = {
        "schema": SCHEMA,
        "phase": "running",
        "manifest_created_at_utc": utc_now(),
        "run_started_at_utc": utc_now(),
        "immutable": immutable,
        "identity_lock_sha256": canonical_hash(immutable),
        "runtime_paths": {
            "repo": str(repo),
            "d0_dataset": str(d0_dataset),
            "d1_corpus": str(d1_corpus),
            **{key: str(path) for key, path in profiles.items()},
        },
        "results": {},
        "artifacts": {},
        "g0_pass": False,
    }
    validate_manifest_data(manifest, check_live=True, require_complete=False)
    write_json_atomic(output, manifest)
    print(f"foundation_manifest_created={output}")
    print(f"identity_lock_sha256={manifest['identity_lock_sha256']}")
    return 0


def validate_manifest_data(
    manifest: dict[str, Any], *, check_live: bool, require_complete: bool
) -> None:
    if manifest.get("schema") != SCHEMA:
        raise ManifestError("unsupported manifest schema")
    if manifest.get("phase") not in {"running", "completed"}:
        raise ManifestError("manifest phase must be running or completed")
    parse_utc(manifest.get("manifest_created_at_utc"), "manifest_created_at_utc")
    parse_utc(manifest.get("run_started_at_utc"), "run_started_at_utc")
    immutable = manifest.get("immutable")
    if not isinstance(immutable, dict):
        raise ManifestError("immutable identity is missing")
    require_sha(manifest.get("identity_lock_sha256"), "identity_lock_sha256")
    if canonical_hash(immutable) != manifest["identity_lock_sha256"]:
        raise ManifestError("immutable identity lock does not match")

    source = immutable.get("source", {})
    if source.get("clean_checkout") is not True:
        raise ManifestError("clean_checkout must be true")
    if not re.fullmatch(r"[0-9a-f]{40}", str(source.get("commit", ""))):
        raise ManifestError("source commit must be a full SHA-1")
    require_sha(source.get("git_status_sha256"), "source.git_status_sha256")
    if source["git_status_sha256"] != sha256_bytes(b""):
        raise ManifestError("source git-status hash is not the empty clean status")
    require_sha(source.get("tracked_tree_sha256"), "source.tracked_tree_sha256")

    benchmark = immutable.get("benchmark", {})
    if benchmark.get("id") != "GDB-DH-AM-1":
        raise ManifestError("benchmark id is wrong")
    require_sha(benchmark.get("source_sha256"), "benchmark.source_sha256")
    require_sha(benchmark.get("generator_sha256"), "benchmark.generator_sha256")
    container = immutable.get("container", {})
    if not IMAGE_DIGEST_RE.fullmatch(str(container.get("digest", ""))):
        raise ManifestError("container digest is missing")
    require_sha(container.get("digest_sha256"), "container.digest_sha256")
    if container.get("documented_uid") != 10001:
        raise ManifestError("documented container uid must be 10001")

    datasets = immutable.get("datasets", {})
    d0 = datasets.get("D0", {})
    require_sha(d0.get("sha256"), "datasets.D0.sha256")
    if not isinstance(d0.get("seed"), int):
        raise ManifestError("D0 seed must be explicit")
    if not isinstance(d0.get("records"), int) or d0["records"] < 20_000:
        raise ManifestError("D0 must contain at least 20000 queries")
    d1 = datasets.get("D1", {})
    if not re.fullmatch(r"[0-9a-f]{40}", str(d1.get("commit", ""))):
        raise ManifestError("D1 commit must be pinned")
    require_sha(d1.get("tree_sha256"), "datasets.D1.tree_sha256")

    for path, label in [
        (("models", "embedding", "sha256"), "embedding checksum"),
        (("models", "generation", "sha256"), "generation checksum"),
        (("prompts", "sha256"), "prompt checksum"),
        (("retrieval", "sha256"), "retrieval checksum"),
        (("profiles", "hardware", "sha256"), "hardware checksum"),
        (("profiles", "tool_permissions", "sha256"), "tool-permissions checksum"),
        (("profiles", "external_versions", "sha256"), "external-versions checksum"),
    ]:
        value: Any = immutable
        for key in path:
            value = value.get(key, {}) if isinstance(value, dict) else None
        require_sha(value, label)
    if not immutable.get("retrieval", {}).get("policy_version"):
        raise ManifestError("retrieval policy version is missing")

    if check_live:
        runtime = manifest.get("runtime_paths", {})
        repo = checked_directory(runtime.get("repo", ""), "live repository")
        clean, status_hash = clean_status(repo)
        if not clean or status_hash != source["git_status_sha256"]:
            raise ManifestError("live source checkout is not clean")
        if run_git(repo, "rev-parse", "HEAD").strip() != source["commit"]:
            raise ManifestError("live source commit changed")
        live_tree, _ = tracked_tree_hash(repo)
        if live_tree != source["tracked_tree_sha256"]:
            raise ManifestError("live source tree hash changed")
        d0_path = checked_file(runtime.get("d0_dataset", ""), "live D0 dataset")
        if sha256_file(d0_path) != d0["sha256"] or jsonl_count(d0_path) != d0["records"]:
            raise ManifestError("live D0 dataset changed")
        d1_path = checked_directory(runtime.get("d1_corpus", ""), "live D1 corpus")
        if run_git(d1_path, "rev-parse", "HEAD").strip() != d1["commit"]:
            raise ManifestError("live D1 commit changed")
        live_d1_tree, _ = directory_tree_hash(d1_path)
        if live_d1_tree != d1["tree_sha256"]:
            raise ManifestError("live D1 tree changed")
        profile_map = {
            "hardware": ("profiles", "hardware"),
            "tool_permissions": ("profiles", "tool_permissions"),
            "external_versions": ("profiles", "external_versions"),
            "retrieval_parameters": ("retrieval",),
        }
        for runtime_key, immutable_path in profile_map.items():
            profile = checked_file(runtime.get(runtime_key, ""), f"live {runtime_key}")
            expected: Any = immutable
            for key in immutable_path:
                expected = expected[key]
            if sha256_file(profile) != expected["sha256"]:
                raise ManifestError(f"live {runtime_key} changed")

    if require_complete:
        if manifest.get("phase") != "completed":
            raise ManifestError("manifest is not finalized")
        results = manifest.get("results")
        if not isinstance(results, dict) or set(results) != REQUIRED_RESULTS:
            raise ManifestError("completed manifest has an incomplete result set")
        if any(value is not True for value in results.values()):
            raise ManifestError("one or more G0 results failed")
        if manifest.get("g0_pass") is not True:
            raise ManifestError("completed manifest does not claim G0 pass")
        artifacts = manifest.get("artifacts")
        if not isinstance(artifacts, dict) or not artifacts:
            raise ManifestError("completed manifest has no artifact hashes")
        for name, entry in artifacts.items():
            if not isinstance(entry, dict):
                raise ManifestError(f"artifact {name} is malformed")
            require_sha(entry.get("sha256"), f"artifact {name} sha256")
            if not isinstance(entry.get("bytes"), int) or entry["bytes"] < 0:
                raise ManifestError(f"artifact {name} byte count is malformed")
        parse_utc(manifest.get("run_finished_at_utc"), "run_finished_at_utc")


def validate_command(args: argparse.Namespace) -> int:
    manifest = load_json(pathlib.Path(args.manifest))
    validate_manifest_data(
        manifest, check_live=args.check_live, require_complete=args.require_complete
    )
    print("foundation_manifest_valid=true")
    print(f"phase={manifest['phase']}")
    print(f"g0_pass={str(manifest.get('g0_pass', False)).lower()}")
    return 0


def parse_result(values: Iterable[str]) -> dict[str, bool]:
    result: dict[str, bool] = {}
    for value in values:
        if "=" not in value:
            raise ManifestError(f"result must be NAME=true|false: {value}")
        name, encoded = value.split("=", 1)
        if name not in REQUIRED_RESULTS or encoded not in {"true", "false"}:
            raise ManifestError(f"invalid G0 result: {value}")
        if name in result:
            raise ManifestError(f"duplicate G0 result: {name}")
        result[name] = encoded == "true"
    if set(result) != REQUIRED_RESULTS:
        missing = sorted(REQUIRED_RESULTS - set(result))
        raise ManifestError(f"missing G0 results: {missing}")
    return result


def finalize_command(args: argparse.Namespace) -> int:
    path = pathlib.Path(args.manifest).resolve()
    manifest = load_json(path)
    validate_manifest_data(manifest, check_live=True, require_complete=False)
    if manifest.get("phase") != "running":
        raise ManifestError("only a running manifest can be finalized")
    results = parse_result(args.result)
    artifacts: dict[str, Any] = {}
    for encoded in args.artifact:
        if "=" not in encoded:
            raise ManifestError("artifact must be NAME=PATH")
        name, value = encoded.split("=", 1)
        if not name or name in artifacts:
            raise ManifestError(f"invalid or duplicate artifact name: {name}")
        artifact = checked_file(value, f"artifact {name}")
        artifacts[name] = {
            "path": str(artifact),
            "sha256": sha256_file(artifact),
            "bytes": artifact.stat().st_size,
        }
    if not artifacts:
        raise ManifestError("at least one evidence artifact is required")
    manifest["phase"] = "completed"
    manifest["run_finished_at_utc"] = utc_now()
    manifest["results"] = results
    manifest["artifacts"] = artifacts
    manifest["g0_pass"] = all(results.values())
    validate_manifest_data(
        manifest, check_live=True, require_complete=manifest["g0_pass"]
    )
    write_json_atomic(path, manifest)
    print(f"foundation_manifest_finalized={path}")
    print(f"g0_pass={str(manifest['g0_pass']).lower()}")
    return 0 if manifest["g0_pass"] else 2


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    commands = root.add_subparsers(dest="command", required=True)

    create = commands.add_parser("create")
    create.add_argument("--repo", required=True)
    create.add_argument("--output", required=True)
    create.add_argument("--run-id", required=True)
    create.add_argument("--d0-dataset", required=True)
    create.add_argument("--d1-corpus", required=True)
    create.add_argument("--d1-commit", required=True)
    create.add_argument("--image-ref", required=True)
    create.add_argument("--image-digest", required=True)
    create.add_argument("--hardware-profile", required=True)
    create.add_argument("--tool-permissions", required=True)
    create.add_argument("--external-versions", required=True)
    create.add_argument("--retrieval-config", required=True)
    create.add_argument("--policy-version", required=True)
    create.add_argument("--seed", required=True, type=int)
    create.set_defaults(handler=create_manifest)

    validate = commands.add_parser("validate")
    validate.add_argument("--manifest", required=True)
    validate.add_argument("--check-live", action="store_true")
    validate.add_argument("--require-complete", action="store_true")
    validate.set_defaults(handler=validate_command)

    finalize = commands.add_parser("finalize")
    finalize.add_argument("--manifest", required=True)
    finalize.add_argument("--result", action="append", default=[], required=True)
    finalize.add_argument("--artifact", action="append", default=[], required=True)
    finalize.set_defaults(handler=finalize_command)
    return root


def main() -> int:
    try:
        args = parser().parse_args()
        return args.handler(args)
    except ManifestError as exc:
        print(f"foundation_manifest_error={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
