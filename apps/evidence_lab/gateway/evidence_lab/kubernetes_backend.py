from __future__ import annotations

import json
from pathlib import Path
import secrets
import shutil
import time
from typing import Any

from .backend import Backend, BackendError, _event
from .config import Settings
from .models import Dataset, PolicyName, Query


class KubernetesJobBackend(Backend):
    """Dispatch one isolated Kubernetes Job for each GrapheneDB run.

    The gateway and worker mount the same private PVC. Only canonical dataset,
    query and policy data are written to the job directory. The worker pod has
    no service token, no ingress and a deny-all NetworkPolicy in the supplied
    deployment manifests.
    """

    def __init__(self, settings: Settings) -> None:
        if not settings.kubernetes_worker_image:
            raise BackendError("EVIDENCE_LAB_KUBERNETES_WORKER_IMAGE is required")
        self.settings = settings
        try:
            from kubernetes import client, config
        except ImportError as exc:
            raise BackendError("install the gateway kubernetes optional dependency") from exc
        self.client = client
        try:
            config.load_incluster_config()
        except config.ConfigException:
            if settings.public_mode:
                raise BackendError("Kubernetes backend requires in-cluster authentication")
            config.load_kube_config()
        self.batch = client.BatchV1Api()

    @staticmethod
    def _job_name() -> str:
        return f"evidence-lab-{secrets.token_hex(10)}"[:63]

    def _job_directory(self, job_name: str) -> Path:
        directory = self.settings.data_dir / "jobs" / job_name
        directory.mkdir(mode=0o700, parents=True, exist_ok=False)
        return directory

    def _build_job(self, job_name: str, input_path: str, output_path: str):
        c = self.client
        labels = {
            "app.kubernetes.io/name": "graphenedb-evidence-lab-worker",
            "app.kubernetes.io/component": "worker",
            "evidence-lab-job": job_name,
        }
        env = [
            c.V1EnvVar(name="EVIDENCE_LAB_JOB_INPUT", value=input_path),
            c.V1EnvVar(name="EVIDENCE_LAB_JOB_OUTPUT", value=output_path),
            c.V1EnvVar(name="EVIDENCE_LAB_BACKEND", value="live"),
            c.V1EnvVar(name="GRAPHENEDB_SERVER_BINARY", value="/opt/graphenedb/bin/graphenedb_server"),
            c.V1EnvVar(name="GRAPHENEDB_SOURCE_COMMIT", value=self.settings.source_commit),
            c.V1EnvVar(name="GRAPHENEDB_PUBLIC_VERSION", value=self.settings.public_version),
            c.V1EnvVar(name="EVIDENCE_LAB_RUN_TIMEOUT_SECONDS", value=str(self.settings.run_timeout_seconds)),
            c.V1EnvVar(name="EVIDENCE_LAB_PUBLIC_MODE", value="true"),
            c.V1EnvVar(name="TMPDIR", value="/tmp"),
        ]
        container = c.V1Container(
            name="worker",
            image=self.settings.kubernetes_worker_image,
            image_pull_policy=self.settings.kubernetes_image_pull_policy,
            command=["python", "-m", "evidence_lab.worker_entrypoint"],
            env=env,
            volume_mounts=[
                c.V1VolumeMount(
                    name="evidence-lab-data",
                    mount_path=self.settings.kubernetes_mount_path,
                ),
                c.V1VolumeMount(name="tmp", mount_path="/tmp"),
            ],
            resources=c.V1ResourceRequirements(
                requests={
                    "cpu": self.settings.kubernetes_worker_cpu_request,
                    "memory": self.settings.kubernetes_worker_memory_request,
                },
                limits={
                    "cpu": self.settings.kubernetes_worker_cpu_limit,
                    "memory": self.settings.kubernetes_worker_memory_limit,
                    "ephemeral-storage": self.settings.kubernetes_worker_ephemeral_storage_limit,
                },
            ),
            security_context=c.V1SecurityContext(
                allow_privilege_escalation=False,
                capabilities=c.V1Capabilities(drop=["ALL"]),
                privileged=False,
                read_only_root_filesystem=True,
                run_as_non_root=True,
                run_as_user=10001,
                run_as_group=10001,
                seccomp_profile=c.V1SeccompProfile(type="RuntimeDefault"),
            ),
        )
        image_pull_secrets = None
        if self.settings.kubernetes_image_pull_secret:
            image_pull_secrets = [
                c.V1LocalObjectReference(
                    name=self.settings.kubernetes_image_pull_secret,
                )
            ]
        pod_spec = c.V1PodSpec(
            restart_policy="Never",
            service_account_name=self.settings.kubernetes_service_account,
            automount_service_account_token=False,
            enable_service_links=False,
            termination_grace_period_seconds=10,
            image_pull_secrets=image_pull_secrets,
            security_context=c.V1PodSecurityContext(
                fs_group=10001,
                run_as_non_root=True,
                run_as_user=10001,
                run_as_group=10001,
                seccomp_profile=c.V1SeccompProfile(type="RuntimeDefault"),
            ),
            containers=[container],
            volumes=[
                c.V1Volume(
                    name="evidence-lab-data",
                    persistent_volume_claim=c.V1PersistentVolumeClaimVolumeSource(
                        claim_name=self.settings.kubernetes_pvc_name,
                        read_only=False,
                    ),
                ),
                c.V1Volume(
                    name="tmp",
                    empty_dir=c.V1EmptyDirVolumeSource(
                        medium="Memory",
                        size_limit=self.settings.kubernetes_worker_ephemeral_storage_limit,
                    ),
                ),
            ],
        )
        template = c.V1PodTemplateSpec(
            metadata=c.V1ObjectMeta(labels=labels),
            spec=pod_spec,
        )
        spec = c.V1JobSpec(
            template=template,
            backoff_limit=0,
            active_deadline_seconds=self.settings.run_timeout_seconds + 30,
            ttl_seconds_after_finished=300,
        )
        return c.V1Job(
            api_version="batch/v1",
            kind="Job",
            metadata=c.V1ObjectMeta(name=job_name, labels=labels),
            spec=spec,
        )

    @staticmethod
    def _read_worker_output(output_file: Path, wait_seconds: float = 10.0) -> dict[str, Any]:
        deadline = time.monotonic() + wait_seconds
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            try:
                if output_file.is_file() and output_file.stat().st_size > 0:
                    return json.loads(output_file.read_text("utf-8"))
            except (OSError, json.JSONDecodeError) as exc:
                last_error = exc
            time.sleep(0.1)
        detail = f": {last_error}" if last_error else ""
        raise BackendError("isolated worker completed without a readable output contract" + detail)

    def run(self, dataset: Dataset, query: Query, policy: PolicyName) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        if policy == PolicyName.previous_stop_reference:
            raise BackendError("previous_stop_reference is recorded-only")
        job_name = self._job_name()
        directory = self._job_directory(job_name)
        input_file = directory / "input.json"
        output_file = directory / "output.json"
        payload = {
            "dataset": dataset.model_dump(mode="json"),
            "query": query.model_dump(mode="json"),
            "policy": policy.value,
        }
        input_file.write_text(json.dumps(payload, sort_keys=True), encoding="utf-8")
        input_file.chmod(0o600)
        input_path = str(Path(self.settings.kubernetes_mount_path) / "jobs" / job_name / "input.json")
        output_path = str(Path(self.settings.kubernetes_mount_path) / "jobs" / job_name / "output.json")
        job = self._build_job(job_name, input_path, output_path)
        events = [
            _event(
                "worker_job_created",
                "Created an isolated Kubernetes Job for this run.",
                job_name=job_name,
                worker_image=self.settings.kubernetes_worker_image,
            )
        ]
        try:
            self.batch.create_namespaced_job(
                namespace=self.settings.kubernetes_namespace,
                body=job,
            )
            deadline = time.monotonic() + self.settings.run_timeout_seconds + 30
            while time.monotonic() < deadline:
                status = self.batch.read_namespaced_job_status(
                    name=job_name,
                    namespace=self.settings.kubernetes_namespace,
                ).status
                if status.succeeded:
                    break
                if status.failed:
                    raise BackendError(f"isolated worker job failed: {job_name}")
                time.sleep(0.5)
            else:
                raise BackendError(f"isolated worker job timed out: {job_name}")
            raw_output = self._read_worker_output(output_file)
            if raw_output.get("error"):
                raise BackendError(str(raw_output["error"]))
            raw = dict(raw_output["raw"])
            raw["_evidence_lab_worker_image_digest"] = self.settings.kubernetes_worker_image
            events.extend(list(raw_output.get("events") or []))
            events.append(
                _event(
                    "worker_job_completed",
                    "Isolated Kubernetes worker returned a GrapheneDB result.",
                    job_name=job_name,
                )
            )
            return raw, events
        finally:
            if self.settings.kubernetes_delete_jobs:
                try:
                    self.batch.delete_namespaced_job(
                        name=job_name,
                        namespace=self.settings.kubernetes_namespace,
                        propagation_policy="Background",
                    )
                except Exception:
                    pass
            shutil.rmtree(directory, ignore_errors=True)
