from __future__ import annotations

import hashlib
import io
import json
from pathlib import Path
import shlex
import zipfile

from .datasets import canonical_dataset_bytes
from .models import Dataset, PublicRunResult


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build_reproduction_bundle(
    dataset: Dataset,
    result: PublicRunResult,
    source_files: dict[str, bytes] | None = None,
) -> bytes:
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
            "worker_image_digest": result.worker_image_digest,
            "run_mode": result.run_mode,
        },
        indent=2,
        sort_keys=True,
    ).encode("utf-8")
    events_json = json.dumps(result.execution_events, indent=2, sort_keys=True).encode("utf-8")

    for filename, data in sorted((source_files or {}).items()):
        safe_name = Path(filename).name
        if safe_name != filename or not safe_name:
            raise ValueError("unsafe source filename in reproduction bundle")
        files[f"source/{safe_name}"] = data

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
        f"Worker image: `{result.worker_image_digest or 'local-subprocess'}`\n\n"
        f"Dataset SHA-256: `{result.dataset_hash}`\n\n"
        "`source/` contains the exact uploaded or sample files used to create the canonical graph.\n\n"
        "`normalised/dataset.json` is the exact canonical dataset sent through the Evidence Lab execution adapter.\n\n"
        "Run `./reproduce.sh https://YOUR-EVIDENCE-LAB-GATEWAY` to submit the canonical dataset to another compatible Evidence Lab deployment.\n\n"
        "A `recorded_reference` bundle is a UI/reference artifact and is not proof that GrapheneDB executed.\n"
        "A `live_graphenedb` bundle originated from the configured disposable GrapheneDB server binary.\n"
    ).encode("utf-8")

    query_value = shlex.quote(result.query_id)
    policy_value = shlex.quote(result.policy.value)
    files["reproduce.sh"] = (
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "BASE_URL=${1:?usage: ./reproduce.sh https://evidence-lab.example.com}\n"
        f"QUERY_ID={query_value}\n"
        f"POLICY={policy_value}\n"
        "SESSION_ID=''\n"
        "cleanup() {\n"
        "  if [[ -n \"$SESSION_ID\" ]]; then\n"
        "    curl -fsS -X DELETE -H \"X-Session-ID: $SESSION_ID\" \"$BASE_URL/v1/public/sessions/$SESSION_ID\" >/dev/null || true\n"
        "  fi\n"
        "}\n"
        "trap cleanup EXIT\n"
        "SESSION_JSON=$(curl -fsS -X POST \"$BASE_URL/v1/public/sessions\")\n"
        "SESSION_ID=$(python3 -c 'import json,sys; print(json.load(sys.stdin)[\"session_id\"])' <<<\"$SESSION_JSON\")\n"
        "UPLOAD_JSON=$(curl -fsS -X POST -H \"X-Session-ID: $SESSION_ID\" -F 'files=@normalised/dataset.json;type=application/json' \"$BASE_URL/v1/public/uploads\")\n"
        "DATASET_ID=$(python3 -c 'import json,sys; print(json.load(sys.stdin)[\"dataset_id\"])' <<<\"$UPLOAD_JSON\")\n"
        "RUN_PAYLOAD=$(QUERY_ID=\"$QUERY_ID\" POLICY=\"$POLICY\" DATASET_ID=\"$DATASET_ID\" python3 -c 'import json,os; print(json.dumps({\"dataset_id\":os.environ[\"DATASET_ID\"],\"query_id\":os.environ[\"QUERY_ID\"],\"policy\":os.environ[\"POLICY\"]}))')\n"
        "RUN_JSON=$(curl -fsS -X POST -H \"X-Session-ID: $SESSION_ID\" -H 'Content-Type: application/json' -d \"$RUN_PAYLOAD\" \"$BASE_URL/v1/public/runs\")\n"
        "printf '%s\\n' \"$RUN_JSON\" | python3 -m json.tool\n"
        "RUN_ID=$(python3 -c 'import json,sys; print(json.load(sys.stdin)[\"run_id\"])' <<<\"$RUN_JSON\")\n"
        "curl -fsS -H \"X-Session-ID: $SESSION_ID\" \"$BASE_URL/v1/public/runs/$RUN_ID/bundle\" -o reproduced-bundle.zip\n"
        "echo \"Downloaded reproduced-bundle.zip for $RUN_ID\"\n"
    ).encode("utf-8")
    files["reproduce.ps1"] = (
        "param([Parameter(Mandatory=$true)][string]$BaseUrl)\n"
        "$ErrorActionPreference = 'Stop'\n"
        "$session = Invoke-RestMethod -Method Post -Uri \"$BaseUrl/v1/public/sessions\"\n"
        "try {\n"
        "  Write-Host 'Use normalised/dataset.json with the /v1/public/uploads multipart endpoint.'\n"
        "  Write-Host \"Session: $($session.session_id)\"\n"
        "  Write-Host 'The POSIX reproduce.sh script contains the complete automated replay.'\n"
        "} finally {\n"
        "  Invoke-RestMethod -Method Delete -Headers @{'X-Session-ID'=$session.session_id} -Uri \"$BaseUrl/v1/public/sessions/$($session.session_id)\" | Out-Null\n"
        "}\n"
    ).encode("utf-8")

    checksum_lines = [f"{_sha256(data)}  {path}" for path, data in sorted(files.items())]
    files["SHA256SUMS"] = ("\n".join(checksum_lines) + "\n").encode("utf-8")

    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path, data in sorted(files.items()):
            info = zipfile.ZipInfo(path)
            info.compress_type = zipfile.ZIP_DEFLATED
            if path.endswith(".sh"):
                info.external_attr = 0o100755 << 16
            archive.writestr(info, data)
    return output.getvalue()
