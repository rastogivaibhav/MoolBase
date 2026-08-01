from __future__ import annotations

import json
import os
from pathlib import Path

os.environ.setdefault("EVIDENCE_LAB_BACKEND", "recorded")
os.environ.setdefault(
    "EVIDENCE_LAB_SAMPLES_DIR",
    str(Path(__file__).resolve().parents[2] / "samples"),
)
os.environ.setdefault("EVIDENCE_LAB_DATA_DIR", "/tmp/graphenedb-evidence-lab-tests")
os.environ.setdefault("EVIDENCE_LAB_ALLOWED_HOSTS", "localhost,127.0.0.1,testserver")

from fastapi.testclient import TestClient

from evidence_lab.app import app


client = TestClient(app)


def create_session() -> str:
    response = client.post("/v1/public/sessions")
    assert response.status_code == 200
    return response.json()["session_id"]


def dataset_payload() -> dict:
    return {
        "manifest": {
            "schema_version": 1,
            "dataset_id": "security-test",
            "title": "Security test",
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
                "source_id": "source-a",
                "evidence_family_id": "family-a",
                "derivation_id": "raw",
                "critical": True,
            }
        ],
        "queries": [
            {"query_id": "q1", "question": "Why did checkout fail?", "target_node": "b"}
        ],
    }


def upload(session_id: str) -> dict:
    response = client.post(
        "/v1/public/uploads",
        headers={"X-Session-ID": session_id},
        files=[
            (
                "files",
                (
                    "dataset.json",
                    json.dumps(dataset_payload()).encode("utf-8"),
                    "application/json",
                ),
            )
        ],
    )
    assert response.status_code == 200, response.text
    return response.json()


def test_security_headers_and_request_id() -> None:
    response = client.get("/v1/public/health")
    assert response.status_code == 200
    assert response.headers["x-content-type-options"] == "nosniff"
    assert response.headers["x-frame-options"] == "DENY"
    assert response.headers["cache-control"] == "no-store"
    assert response.headers["x-request-id"].startswith("req_")


def test_untrusted_host_is_rejected_but_health_probe_is_allowed() -> None:
    rejected = client.post("/v1/public/sessions", headers={"Host": "attacker.invalid"})
    assert rejected.status_code == 400
    assert rejected.json()["error"] == "invalid_host"

    health = client.get("/v1/public/health", headers={"Host": "10.10.10.10:8080"})
    assert health.status_code == 200


def test_upload_reports_security_pass() -> None:
    session_id = create_session()
    result = upload(session_id)
    assert result["security"]["passed"] is True
    assert result["security"]["files_scanned"] == 1


def test_credential_upload_is_rejected_before_parsing() -> None:
    session_id = create_session()
    payload = b'{"credential":"sk-proj-abcdefghijklmnopqrstuvwxyz123456"}'
    response = client.post(
        "/v1/public/uploads",
        headers={"X-Session-ID": session_id},
        files=[("files", ("dataset.json", payload, "application/json"))],
    )
    assert response.status_code == 422
    assert response.json()["error"] == "upload_security_rejection"


def test_cross_session_dataset_and_run_access_is_denied() -> None:
    owner = create_session()
    other = create_session()
    uploaded = upload(owner)

    denied_dataset = client.get(
        f"/v1/public/datasets/{uploaded['dataset_id']}",
        headers={"X-Session-ID": other},
    )
    assert denied_dataset.status_code == 404

    run_response = client.post(
        "/v1/public/runs",
        headers={"X-Session-ID": owner},
        json={
            "dataset_id": uploaded["dataset_id"],
            "query_id": "q1",
            "policy": "frontier_aware",
        },
    )
    assert run_response.status_code == 200, run_response.text
    run_id = run_response.json()["run_id"]

    denied_run = client.get(
        f"/v1/public/runs/{run_id}",
        headers={"X-Session-ID": other},
    )
    denied_bundle = client.get(
        f"/v1/public/runs/{run_id}/bundle",
        headers={"X-Session-ID": other},
    )
    assert denied_run.status_code == 404
    assert denied_bundle.status_code == 404


def test_delete_requires_matching_session_header() -> None:
    owner = create_session()
    other = create_session()
    response = client.delete(
        f"/v1/public/sessions/{owner}",
        headers={"X-Session-ID": other},
    )
    assert response.status_code == 403
