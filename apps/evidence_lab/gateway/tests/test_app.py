from __future__ import annotations

import io
import json
import os
from pathlib import Path
import zipfile

os.environ.setdefault("EVIDENCE_LAB_BACKEND", "recorded")
os.environ.setdefault(
    "EVIDENCE_LAB_SAMPLES_DIR",
    str(Path(__file__).resolve().parents[2] / "samples"),
)
os.environ.setdefault("EVIDENCE_LAB_DATA_DIR", "/tmp/graphenedb-evidence-lab-tests")

from fastapi.testclient import TestClient

from evidence_lab.app import app


client = TestClient(app)


def session() -> str:
    response = client.post("/v1/public/sessions")
    assert response.status_code == 200
    return response.json()["session_id"]


def sample_json() -> dict:
    return {
        "manifest": {
            "schema_version": 1,
            "dataset_id": "test-upload",
            "title": "Test upload",
            "licence": "user-supplied",
        },
        "nodes": [
            {"external_id": "a", "text": "release changed timeout", "role": "root"},
            {"external_id": "b", "text": "checkout failures", "role": "symptom"},
        ],
        "edges": [
            {
                "edge_id": "e1",
                "from_node": "a",
                "to_node": "b",
                "role": "causal",
                "source_id": "s1",
                "evidence_family_id": "f1",
                "derivation_id": "raw",
                "critical": True,
            }
        ],
        "queries": [
            {"query_id": "q1", "question": "Why did checkout fail?", "target_node": "b"}
        ],
    }


def test_health_and_samples() -> None:
    assert client.get("/v1/public/health").status_code == 200
    response = client.get("/v1/public/samples")
    assert response.status_code == 200
    ids = {item["sample_id"] for item in response.json()["samples"]}
    assert "incident-hidden-hop" in ids


def test_upload_run_and_bundle() -> None:
    session_id = session()
    headers = {"X-Session-ID": session_id}
    data = json.dumps(sample_json()).encode("utf-8")
    response = client.post(
        "/v1/public/uploads",
        headers=headers,
        files=[("files", ("dataset.json", data, "application/json"))],
    )
    assert response.status_code == 200, response.text
    upload = response.json()
    assert upload["node_count"] == 2

    response = client.post(
        "/v1/public/runs",
        headers=headers,
        json={"dataset_id": upload["dataset_id"], "query_id": "q1", "policy": "frontier_aware"},
    )
    assert response.status_code == 200, response.text
    run = response.json()
    assert run["run_mode"] == "recorded_reference"
    assert run["live"] is False
    assert run["receipt"]["recorded_reference"] is True

    bundle = client.get(f"/v1/public/runs/{run['run_id']}/bundle", headers=headers)
    assert bundle.status_code == 200
    with zipfile.ZipFile(io.BytesIO(bundle.content)) as archive:
        names = set(archive.namelist())
    assert "normalised/dataset.json" in names
    assert "result/compact-receipt.json" in names
    assert "SHA256SUMS" in names


def test_rejects_unresolved_edge() -> None:
    session_id = session()
    payload = sample_json()
    payload["edges"][0]["to_node"] = "missing"
    response = client.post(
        "/v1/public/uploads",
        headers={"X-Session-ID": session_id},
        files=[("files", ("dataset.json", json.dumps(payload).encode(), "application/json"))],
    )
    assert response.status_code == 422
    assert "unresolved references" in response.text


def test_sample_run_and_delete_session() -> None:
    session_id = session()
    headers = {"X-Session-ID": session_id}
    response = client.post(
        "/v1/public/runs",
        headers=headers,
        json={"sample_id": "incident-hidden-hop", "query_id": "why-checkout", "policy": "one_pass"},
    )
    assert response.status_code == 200, response.text
    assert response.json()["dataset_id"] == "incident-hidden-hop"
    deleted = client.delete(f"/v1/public/sessions/{session_id}", headers=headers)
    assert deleted.status_code == 204
