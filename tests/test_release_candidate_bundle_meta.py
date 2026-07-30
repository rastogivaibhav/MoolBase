#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def write_file(path: pathlib.Path, text: str = "x") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def run_validator(validator: pathlib.Path, metadata: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(validator), str(metadata)],
        check=False,
        text=True,
        capture_output=True,
    )


def build_valid_metadata(bundle_dir: pathlib.Path, package_path: pathlib.Path) -> pathlib.Path:
    status_report = bundle_dir / "GA_STATUS_REPORT.md"
    ga_summary = bundle_dir / "ga-readiness" / "GA_READINESS_SUMMARY.md"
    evidence_bundle = bundle_dir / "ga-evidence"
    evidence_archive = bundle_dir / "ga-evidence.zip"
    package_sha = package_path.with_suffix(package_path.suffix + ".sha256")
    package_manifest = package_path.with_suffix(package_path.suffix + ".manifest.json")

    for path in (status_report, ga_summary, evidence_bundle, evidence_archive, package_path, package_sha, package_manifest):
        write_file(path, f"{path.name}\n")

    meta = {
        "generated_at_utc": "2026-07-05T00:00:00Z",
        "profile_label": "release-candidate-smoke",
        "status_report": str(status_report),
        "ga_summary": str(ga_summary),
        "evidence_bundle": str(evidence_bundle),
        "evidence_archive": str(evidence_archive),
        "package_path": str(package_path),
        "package_sha256": str(package_sha),
        "package_manifest": str(package_manifest),
        "requested": {
            "vector_index_recall_kind": "auto",
            "extraction_vector_index": "flat",
            "storage_vector_index": "kdtree",
            "use_faiss": False,
        },
        "resolved": {
            "vector_index_recall": "kdtree",
            "vector_index_recall_requested": "auto",
            "vector_index_recall_mean_recall_at_k": "1",
            "extraction_vector_index": "flat",
            "extraction_vector_index_requested": "flat",
            "storage_vector_index": "kdtree",
            "storage_vector_index_requested": "kdtree",
            "graphenedb_use_faiss": "False",
        },
    }
    metadata = bundle_dir / "RELEASE_CANDIDATE_BUNDLE_META.json"
    metadata.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    return metadata


def build_invalid_metadata(bundle_dir: pathlib.Path) -> pathlib.Path:
    metadata = bundle_dir / "BROKEN_RELEASE_CANDIDATE_BUNDLE_META.json"
    metadata.write_text(
        json.dumps(
            {
                "generated_at_utc": "2026-07-05T00:00:00Z",
                "profile_label": "release-candidate-smoke",
                "requested": {},
                "resolved": {},
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return metadata


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_release_candidate_bundle_meta.py <validator.py>", file=sys.stderr)
        return 2

    validator = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="graphenedb_bundle_meta_") as tmp:
        bundle_dir = pathlib.Path(tmp)
        package_path = bundle_dir / "graphenedb-install-package.zip"
        valid_metadata = build_valid_metadata(bundle_dir, package_path)
        valid_result = run_validator(validator, valid_metadata)
        if valid_result.returncode != 0:
            print(valid_result.stdout, file=sys.stderr)
            print(valid_result.stderr, file=sys.stderr)
            print("validator unexpectedly rejected valid metadata", file=sys.stderr)
            return 1

        invalid_metadata = build_invalid_metadata(bundle_dir)
        invalid_result = run_validator(validator, invalid_metadata)
        if invalid_result.returncode == 0:
            print("validator unexpectedly accepted invalid metadata", file=sys.stderr)
            return 1
        if "missing top-level fields" not in invalid_result.stderr:
            print(invalid_result.stderr, file=sys.stderr)
            print("validator did not report missing top-level fields", file=sys.stderr)
            return 1

    print("release_candidate_bundle_meta_validator_passed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
