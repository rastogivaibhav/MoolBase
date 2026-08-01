from __future__ import annotations

from pathlib import Path

from kubernetes import client, config

from evidence_lab.config import Settings
from evidence_lab.kubernetes_backend import KubernetesJobBackend


IMMUTABLE_WORKER = (
    "ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker@sha256:"
    + "a" * 64
)


def test_generated_job_matches_admission_contract(monkeypatch, tmp_path: Path) -> None:
    monkeypatch.setenv("EVIDENCE_LAB_PUBLIC_MODE", "true")
    monkeypatch.setenv("EVIDENCE_LAB_BACKEND", "kubernetes")
    monkeypatch.setenv("EVIDENCE_LAB_DATA_DIR", str(tmp_path))
    monkeypatch.setenv("EVIDENCE_LAB_KUBERNETES_WORKER_IMAGE", IMMUTABLE_WORKER)
    monkeypatch.setenv("GRAPHENEDB_SOURCE_COMMIT", "b" * 40)
    monkeypatch.setenv("GRAPHENEDB_PUBLIC_VERSION", "v0.6.0-alpha.1-test")
    monkeypatch.setattr(config, "load_incluster_config", lambda: None)
    monkeypatch.setattr(client, "BatchV1Api", lambda: object())

    backend = KubernetesJobBackend(Settings.from_env())
    job = backend._build_job(
        "evidence-lab-" + "1" * 20,
        "/var/lib/evidence-lab/jobs/evidence-lab-" + "1" * 20 + "/input.json",
        "/var/lib/evidence-lab/jobs/evidence-lab-" + "1" * 20 + "/output.json",
    ).to_dict()

    assert job["metadata"]["namespace"] is None
    assert job["metadata"]["labels"]["app.kubernetes.io/name"] == "graphenedb-evidence-lab-worker"
    assert job["metadata"]["labels"]["app.kubernetes.io/component"] == "worker"

    spec = job["spec"]
    pod = spec["template"]["spec"]
    worker = pod["containers"][0]

    assert spec["backoff_limit"] == 0
    assert spec["active_deadline_seconds"] <= 150
    assert spec["ttl_seconds_after_finished"] <= 300
    assert pod["service_account_name"] == "evidence-lab-worker"
    assert pod["automount_service_account_token"] is False
    assert pod["restart_policy"] == "Never"
    assert pod["enable_service_links"] is False
    assert len(pod["containers"]) == 1
    assert worker["name"] == "worker"
    assert worker["image"] == IMMUTABLE_WORKER
    assert worker["command"] == ["python", "-m", "evidence_lab.worker_entrypoint"]

    security = worker["security_context"]
    assert security["run_as_non_root"] is True
    assert security["allow_privilege_escalation"] is False
    assert security["privileged"] is False
    assert security["read_only_root_filesystem"] is True
    assert "ALL" in security["capabilities"]["drop"]
    assert security["seccomp_profile"]["type"] == "RuntimeDefault"

    volumes = {item["name"]: item for item in pod["volumes"]}
    assert set(volumes) == {"evidence-lab-data", "tmp"}
    assert volumes["evidence-lab-data"]["persistent_volume_claim"]["claim_name"] == "evidence-lab-data"
    assert volumes["tmp"]["empty_dir"]["medium"] == "Memory"
