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
    job_name = "evidence-lab-" + "1" * 20
    input_path = f"/var/lib/evidence-lab/jobs/{job_name}/input.json"
    output_path = f"/var/lib/evidence-lab/jobs/{job_name}/output.json"
    job = backend._build_job(job_name, input_path, output_path).to_dict()

    assert job["metadata"]["namespace"] is None
    assert job["metadata"]["labels"]["app.kubernetes.io/name"] == "graphenedb-evidence-lab-worker"
    assert job["metadata"]["labels"]["app.kubernetes.io/component"] == "worker"

    spec = job["spec"]
    template = spec["template"]
    assert template["metadata"]["labels"]["app.kubernetes.io/name"] == "graphenedb-evidence-lab-worker"
    assert template["metadata"]["labels"]["app.kubernetes.io/component"] == "worker"
    pod = template["spec"]
    worker = pod["containers"][0]

    assert spec["backoff_limit"] == 0
    assert spec["active_deadline_seconds"] <= 150
    assert spec["ttl_seconds_after_finished"] <= 300
    assert pod["service_account_name"] == "evidence-lab-worker"
    assert pod["automount_service_account_token"] is False
    assert pod["restart_policy"] == "Never"
    assert pod["enable_service_links"] is False
    assert not pod["init_containers"]
    assert not pod["ephemeral_containers"]
    assert pod["host_network"] in {None, False}
    assert pod["host_pid"] in {None, False}
    assert pod["host_ipc"] in {None, False}
    assert pod["share_process_namespace"] in {None, False}
    assert len(pod["containers"]) == 1

    pod_security = pod["security_context"]
    assert pod_security["run_as_non_root"] is True
    assert pod_security["run_as_user"] == 10001
    assert pod_security["run_as_group"] == 10001
    assert pod_security["fs_group"] == 10001
    assert pod_security["seccomp_profile"]["type"] == "RuntimeDefault"

    assert worker["name"] == "worker"
    assert worker["image"] == IMMUTABLE_WORKER
    assert worker["command"] == ["python", "-m", "evidence_lab.worker_entrypoint"]
    assert worker["args"] is None
    assert worker["lifecycle"] is None
    assert worker["liveness_probe"] is None
    assert worker["readiness_probe"] is None
    assert worker["startup_probe"] is None

    environment = {item["name"]: item["value"] for item in worker["env"]}
    assert environment == {
        "EVIDENCE_LAB_JOB_INPUT": input_path,
        "EVIDENCE_LAB_JOB_OUTPUT": output_path,
        "EVIDENCE_LAB_BACKEND": "live",
        "GRAPHENEDB_SERVER_BINARY": "/opt/graphenedb/bin/graphenedb_server",
        "GRAPHENEDB_SOURCE_COMMIT": "b" * 40,
        "GRAPHENEDB_PUBLIC_VERSION": "v0.6.0-alpha.1-test",
        "EVIDENCE_LAB_RUN_TIMEOUT_SECONDS": "120",
        "EVIDENCE_LAB_PUBLIC_MODE": "true",
        "TMPDIR": "/tmp",
    }
    assert all(item["value_from"] is None for item in worker["env"])

    security = worker["security_context"]
    assert security["run_as_non_root"] is True
    assert security["run_as_user"] == 10001
    assert security["run_as_group"] == 10001
    assert security["allow_privilege_escalation"] is False
    assert security["privileged"] is False
    assert security["read_only_root_filesystem"] is True
    assert "ALL" in security["capabilities"]["drop"]
    assert security["seccomp_profile"]["type"] == "RuntimeDefault"

    resources = worker["resources"]
    assert set(resources["requests"]) == {"cpu", "memory"}
    assert set(resources["limits"]) == {"cpu", "memory", "ephemeral-storage"}

    mounts = {item["name"]: item for item in worker["volume_mounts"]}
    assert set(mounts) == {"evidence-lab-data", "tmp"}
    assert mounts["evidence-lab-data"]["mount_path"] == "/var/lib/evidence-lab"
    assert mounts["tmp"]["mount_path"] == "/tmp"

    volumes = {item["name"]: item for item in pod["volumes"]}
    assert set(volumes) == {"evidence-lab-data", "tmp"}
    assert volumes["evidence-lab-data"]["persistent_volume_claim"]["claim_name"] == "evidence-lab-data"
    assert volumes["tmp"]["empty_dir"]["medium"] == "Memory"
    assert volumes["tmp"]["empty_dir"]["size_limit"] == "512Mi"
