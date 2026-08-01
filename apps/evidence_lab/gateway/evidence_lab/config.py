from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path


def _bool(name: str, default: bool) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


@dataclass(frozen=True)
class Settings:
    data_dir: Path
    samples_dir: Path
    backend_mode: str
    graphenedb_server_binary: str | None
    graphenedb_dimension: int
    session_ttl_seconds: int
    max_upload_bytes: int
    max_nodes: int
    max_edges: int
    max_queries: int
    run_timeout_seconds: int
    allowed_origins: tuple[str, ...]
    allowed_hosts: tuple[str, ...]
    source_commit: str
    public_version: str
    public_mode: bool
    max_upload_files: int
    max_filename_length: int
    max_archive_members: int
    max_archive_member_bytes: int
    max_archive_uncompressed_bytes: int
    max_archive_compression_ratio: float
    block_secrets: bool
    clamav_host: str | None
    clamav_port: int
    clamav_required: bool
    clamav_timeout_seconds: float
    rate_limit_requests: int
    rate_limit_window_seconds: int
    worker_memory_bytes: int
    worker_cpu_seconds: int
    worker_open_files: int
    worker_processes: int
    worker_file_bytes: int
    kubernetes_namespace: str
    kubernetes_worker_image: str | None
    kubernetes_pvc_name: str
    kubernetes_mount_path: str
    kubernetes_service_account: str
    kubernetes_image_pull_policy: str
    kubernetes_image_pull_secret: str | None
    kubernetes_worker_cpu_request: str
    kubernetes_worker_cpu_limit: str
    kubernetes_worker_memory_request: str
    kubernetes_worker_memory_limit: str
    kubernetes_worker_ephemeral_storage_limit: str
    kubernetes_delete_jobs: bool

    @classmethod
    def from_env(cls) -> "Settings":
        module_path = Path(__file__).resolve()
        parents = module_path.parents
        default_root = parents[3] if len(parents) > 3 else module_path.parent.parent
        samples_dir = Path(
            os.environ.get(
                "EVIDENCE_LAB_SAMPLES_DIR",
                default_root / "samples",
            )
        ).resolve()
        data_dir = Path(
            os.environ.get("EVIDENCE_LAB_DATA_DIR", "/tmp/graphenedb-evidence-lab")
        ).resolve()
        origins = tuple(
            item.strip()
            for item in os.environ.get(
                "EVIDENCE_LAB_ALLOWED_ORIGINS", "http://localhost:8088,http://127.0.0.1:8088"
            ).split(",")
            if item.strip()
        )
        hosts = tuple(
            item.strip()
            for item in os.environ.get("EVIDENCE_LAB_ALLOWED_HOSTS", "localhost,127.0.0.1,testserver").split(",")
            if item.strip()
        )
        max_upload = int(os.environ.get("EVIDENCE_LAB_MAX_UPLOAD_BYTES", str(5 * 1024 * 1024)))
        pull_secret = os.environ.get("EVIDENCE_LAB_KUBERNETES_IMAGE_PULL_SECRET", "ghcr-pull").strip()
        return cls(
            data_dir=data_dir,
            samples_dir=samples_dir,
            backend_mode=os.environ.get("EVIDENCE_LAB_BACKEND", "recorded").strip().lower(),
            graphenedb_server_binary=os.environ.get("GRAPHENEDB_SERVER_BINARY"),
            graphenedb_dimension=int(os.environ.get("GRAPHENEDB_DIMENSION", "16")),
            session_ttl_seconds=int(os.environ.get("EVIDENCE_LAB_SESSION_TTL_SECONDS", "3600")),
            max_upload_bytes=max_upload,
            max_nodes=int(os.environ.get("EVIDENCE_LAB_MAX_NODES", "5000")),
            max_edges=int(os.environ.get("EVIDENCE_LAB_MAX_EDGES", "20000")),
            max_queries=int(os.environ.get("EVIDENCE_LAB_MAX_QUERIES", "5")),
            run_timeout_seconds=int(os.environ.get("EVIDENCE_LAB_RUN_TIMEOUT_SECONDS", "120")),
            allowed_origins=origins,
            allowed_hosts=hosts,
            source_commit=os.environ.get("GRAPHENEDB_SOURCE_COMMIT", "unverified-development-head"),
            public_version=os.environ.get("GRAPHENEDB_PUBLIC_VERSION", "v0.6.0-alpha.1-dev"),
            public_mode=_bool("EVIDENCE_LAB_PUBLIC_MODE", False),
            max_upload_files=int(os.environ.get("EVIDENCE_LAB_MAX_UPLOAD_FILES", "8")),
            max_filename_length=int(os.environ.get("EVIDENCE_LAB_MAX_FILENAME_LENGTH", "180")),
            max_archive_members=int(os.environ.get("EVIDENCE_LAB_MAX_ARCHIVE_MEMBERS", "64")),
            max_archive_member_bytes=int(os.environ.get("EVIDENCE_LAB_MAX_ARCHIVE_MEMBER_BYTES", str(max_upload))),
            max_archive_uncompressed_bytes=int(os.environ.get("EVIDENCE_LAB_MAX_ARCHIVE_UNCOMPRESSED_BYTES", str(max_upload * 4))),
            max_archive_compression_ratio=float(os.environ.get("EVIDENCE_LAB_MAX_ARCHIVE_COMPRESSION_RATIO", "100")),
            block_secrets=_bool("EVIDENCE_LAB_BLOCK_SECRETS", True),
            clamav_host=os.environ.get("EVIDENCE_LAB_CLAMAV_HOST"),
            clamav_port=int(os.environ.get("EVIDENCE_LAB_CLAMAV_PORT", "3310")),
            clamav_required=_bool("EVIDENCE_LAB_CLAMAV_REQUIRED", False),
            clamav_timeout_seconds=float(os.environ.get("EVIDENCE_LAB_CLAMAV_TIMEOUT_SECONDS", "8")),
            rate_limit_requests=int(os.environ.get("EVIDENCE_LAB_RATE_LIMIT_REQUESTS", "120")),
            rate_limit_window_seconds=int(os.environ.get("EVIDENCE_LAB_RATE_LIMIT_WINDOW_SECONDS", "60")),
            worker_memory_bytes=int(os.environ.get("EVIDENCE_LAB_WORKER_MEMORY_BYTES", str(768 * 1024 * 1024))),
            worker_cpu_seconds=int(os.environ.get("EVIDENCE_LAB_WORKER_CPU_SECONDS", "90")),
            worker_open_files=int(os.environ.get("EVIDENCE_LAB_WORKER_OPEN_FILES", "128")),
            worker_processes=int(os.environ.get("EVIDENCE_LAB_WORKER_PROCESSES", "32")),
            worker_file_bytes=int(os.environ.get("EVIDENCE_LAB_WORKER_FILE_BYTES", str(64 * 1024 * 1024))),
            kubernetes_namespace=os.environ.get("EVIDENCE_LAB_KUBERNETES_NAMESPACE", "graphenedb-evidence-lab"),
            kubernetes_worker_image=os.environ.get("EVIDENCE_LAB_KUBERNETES_WORKER_IMAGE"),
            kubernetes_pvc_name=os.environ.get("EVIDENCE_LAB_KUBERNETES_PVC", "evidence-lab-data"),
            kubernetes_mount_path=os.environ.get("EVIDENCE_LAB_KUBERNETES_MOUNT_PATH", "/var/lib/evidence-lab"),
            kubernetes_service_account=os.environ.get("EVIDENCE_LAB_KUBERNETES_SERVICE_ACCOUNT", "evidence-lab-worker"),
            kubernetes_image_pull_policy=os.environ.get("EVIDENCE_LAB_KUBERNETES_IMAGE_PULL_POLICY", "IfNotPresent"),
            kubernetes_image_pull_secret=pull_secret or None,
            kubernetes_worker_cpu_request=os.environ.get("EVIDENCE_LAB_WORKER_CPU_REQUEST", "250m"),
            kubernetes_worker_cpu_limit=os.environ.get("EVIDENCE_LAB_WORKER_CPU_LIMIT", "1"),
            kubernetes_worker_memory_request=os.environ.get("EVIDENCE_LAB_WORKER_MEMORY_REQUEST", "256Mi"),
            kubernetes_worker_memory_limit=os.environ.get("EVIDENCE_LAB_WORKER_MEMORY_LIMIT", "1Gi"),
            kubernetes_worker_ephemeral_storage_limit=os.environ.get("EVIDENCE_LAB_WORKER_EPHEMERAL_STORAGE_LIMIT", "512Mi"),
            kubernetes_delete_jobs=_bool("EVIDENCE_LAB_KUBERNETES_DELETE_JOBS", True),
        )
