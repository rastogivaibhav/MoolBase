#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import sys


REQUIRED_TOP_LEVEL = {
    "generated_at_utc",
    "profile_label",
    "status_report",
    "ga_summary",
    "evidence_bundle",
    "evidence_archive",
    "package_path",
    "package_sha256",
    "package_manifest",
    "requested",
    "resolved",
}

REQUIRED_REQUESTED = {
    "vector_index_recall_kind",
    "extraction_vector_index",
    "storage_vector_index",
    "use_faiss",
}

REQUIRED_RESOLVED = {
    "vector_index_recall",
    "vector_index_recall_requested",
    "vector_index_recall_mean_recall_at_k",
    "extraction_vector_index",
    "extraction_vector_index_requested",
    "storage_vector_index",
    "storage_vector_index_requested",
    "graphenedb_use_faiss",
}


def fail(message: str) -> int:
    print(f"release_candidate_bundle_meta_invalid: {message}", file=sys.stderr)
    return 1


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: validate_release_candidate_bundle_meta.py <metadata.json>")

    path = pathlib.Path(sys.argv[1])
    if not path.exists():
        return fail(f"metadata not found: {path}")

    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as exc:  # pragma: no cover - defensive
        return fail(f"failed to parse JSON: {exc}")

    if not isinstance(data, dict):
        return fail("metadata must be a JSON object")

    missing_top = sorted(REQUIRED_TOP_LEVEL - data.keys())
    if missing_top:
        return fail(f"missing top-level fields: {', '.join(missing_top)}")

    for key in ("requested", "resolved"):
        if not isinstance(data[key], dict):
            return fail(f"{key} must be a JSON object")

    missing_requested = sorted(REQUIRED_REQUESTED - data["requested"].keys())
    if missing_requested:
        return fail(f"missing requested fields: {', '.join(missing_requested)}")

    missing_resolved = sorted(REQUIRED_RESOLVED - data["resolved"].keys())
    if missing_resolved:
        return fail(f"missing resolved fields: {', '.join(missing_resolved)}")

    for key in ("status_report", "ga_summary", "evidence_bundle", "evidence_archive", "package_path", "package_sha256", "package_manifest"):
        value = data.get(key, "")
        if not isinstance(value, str) or not value.strip():
            return fail(f"{key} must be a non-empty string")

    for key in ("status_report", "ga_summary", "evidence_bundle", "evidence_archive", "package_path", "package_sha256", "package_manifest"):
        target = pathlib.Path(data[key])
        if not target.exists():
            return fail(f"{key} path not found: {target}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
