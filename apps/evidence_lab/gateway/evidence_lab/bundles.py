from __future__ import annotations

import hashlib
import io
import json
import zipfile

from .datasets import canonical_dataset_bytes
from .models import Dataset, PublicRunResult


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build_reproduction_bundle(dataset: Dataset, result: PublicRunResult) -> bytes:
    files: dict[str, bytes] = {}
    dataset_json = canonical_dataset_bytes(dataset)
    result_json = json.dumps(result.model_dump(mode="json"), indent=2, sort_keys=True).encode("utf-8")
    receipt_json = json.dumps(result.receipt, indent=2, sort_keys=True).encode("utf-8")
    graph_json = json.dumps(result.graph, indent=2, sort_keys=True).encode("utf-8")
    raw_json = json.dumps(result.raw_engine_result, indent=2, sort_keys=True).encode("utf-8")
    config_json = json.dumps(
        {
            "query_id": result.query_id,
            "question": result.question,
            "policy": result.policy.value,
            "dataset_hash": result.dataset_hash,
            "graphenedb_version": result.graphenedb_version,
            "graphenedb_commit": result.graphenedb_commit,
            "run_mode": result.run_mode,
        },
        indent=2,
        sort_keys=True,
    ).encode("utf-8")
    events_json = json.dumps(result.execution_events, indent=2, sort_keys=True).encode("utf-8")

    files["normalised/dataset.json"] = dataset_json
    files["execution/configuration.json"] = config_json
    files["execution/events.json"] = events_json
    files["execution/raw-engine-result.json"] = raw_json
    files["result/public-result.json"] = result_json
    files["result/compact-receipt.json"] = receipt_json
    files["result/evidence-graph.json"] = graph_json
    files["README.md"] = (
        f"# GrapheneDB Evidence Lab run {result.run_id}\n\n"
        f"Run mode: `{result.run_mode}`\n\n"
        f"GrapheneDB version: `{result.graphenedb_version}`\n\n"
        f"Source commit: `{result.graphenedb_commit}`\n\n"
        f"Dataset SHA-256: `{result.dataset_hash}`\n\n"
        "A `recorded_reference` bundle is a UI/reference artifact and is not proof that GrapheneDB executed.\n"
        "A `live_graphenedb` bundle originated from the configured disposable GrapheneDB server binary.\n"
    ).encode("utf-8")
    files["reproduce.sh"] = (
        "#!/usr/bin/env bash\nset -euo pipefail\n"
        "echo 'This bundle preserves canonical input and output.'\n"
        "echo 'Use the immutable GrapheneDB release documented in execution/configuration.json.'\n"
        "echo 'Import normalised/dataset.json through the Evidence Lab gateway or the release CLI/API adapter.'\n"
    ).encode("utf-8")
    files["reproduce.ps1"] = (
        "$ErrorActionPreference = 'Stop'\n"
        "Write-Host 'This bundle preserves canonical input and output.'\n"
        "Write-Host 'Use the immutable GrapheneDB release documented in execution/configuration.json.'\n"
    ).encode("utf-8")

    checksum_lines = [f"{_sha256(data)}  {path}" for path, data in sorted(files.items())]
    files["SHA256SUMS"] = ("\n".join(checksum_lines) + "\n").encode("utf-8")

    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path, data in sorted(files.items()):
            archive.writestr(path, data)
    return output.getvalue()
