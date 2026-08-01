from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path


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
    source_commit: str
    public_version: str

    @classmethod
    def from_env(cls) -> "Settings":
        default_root = Path(__file__).resolve().parents[3]
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
        return cls(
            data_dir=data_dir,
            samples_dir=samples_dir,
            backend_mode=os.environ.get("EVIDENCE_LAB_BACKEND", "recorded").strip().lower(),
            graphenedb_server_binary=os.environ.get("GRAPHENEDB_SERVER_BINARY"),
            graphenedb_dimension=int(os.environ.get("GRAPHENEDB_DIMENSION", "16")),
            session_ttl_seconds=int(os.environ.get("EVIDENCE_LAB_SESSION_TTL_SECONDS", "3600")),
            max_upload_bytes=int(os.environ.get("EVIDENCE_LAB_MAX_UPLOAD_BYTES", str(5 * 1024 * 1024))),
            max_nodes=int(os.environ.get("EVIDENCE_LAB_MAX_NODES", "5000")),
            max_edges=int(os.environ.get("EVIDENCE_LAB_MAX_EDGES", "20000")),
            max_queries=int(os.environ.get("EVIDENCE_LAB_MAX_QUERIES", "5")),
            run_timeout_seconds=int(os.environ.get("EVIDENCE_LAB_RUN_TIMEOUT_SECONDS", "120")),
            allowed_origins=origins,
            source_commit=os.environ.get("GRAPHENEDB_SOURCE_COMMIT", "unverified-development-head"),
            public_version=os.environ.get("GRAPHENEDB_PUBLIC_VERSION", "v0.6.0-alpha.1-dev"),
        )
