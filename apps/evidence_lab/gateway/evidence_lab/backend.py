from __future__ import annotations

from abc import ABC, abstractmethod
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import secrets
import shutil
import signal
import socket
import subprocess
import tempfile
import time
from typing import Any, Iterator

import httpx

from .config import Settings
from .datasets import dataset_hash
from .models import Dataset, PolicyName, Query


class BackendError(RuntimeError):
    pass


class Backend(ABC):
    @abstractmethod
    def run(self, dataset: Dataset, query: Query, policy: PolicyName) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        raise NotImplementedError


def _event(stage: str, detail: str, **extra: Any) -> dict[str, Any]:
    return {
        "at": datetime.now(timezone.utc).isoformat(),
        "stage": stage,
        "detail": detail,
        **extra,
    }


class RecordedReferenceBackend(Backend):
    """Deterministic recorded output for UI development and disconnected preview.

    This mode is intentionally labelled recorded_reference by the public contract.
    It never claims that GrapheneDB executed.
    """

    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def run(self, dataset: Dataset, query: Query, policy: PolicyName) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        digest = hashlib.sha256(
            f"{dataset_hash(dataset)}:{query.query_id}:{policy.value}".encode("utf-8")
        ).hexdigest()
        if policy == PolicyName.one_pass:
            status, confidence, cycles, visited, edges, energy = "evidence_required", 0.54, 0, 5, 4, 0.68
        elif policy == PolicyName.frontier_aware:
            status, confidence, cycles, visited, edges, energy = "provisionally_resolved", 0.88, 2, 16, 12, 0.09
        elif policy == PolicyName.broad_expansion:
            status, confidence, cycles, visited, edges, energy = "contested", 0.74, 3, 34, 27, 0.31
        else:
            status, confidence, cycles, visited, edges, energy = "provisionally_resolved", 0.66, 2, 10, 8, 0.22
        primary = query.target_node or (dataset.nodes[-1].external_id if dataset.nodes else None)
        selected = [edge.edge_id for edge in dataset.edges if edge.role in {"supports", "causal", "mechanistic"}][:4]
        events = [
            _event("recorded_reference", "Recorded reference mode active; GrapheneDB was not executed."),
            _event("dataset_loaded", f"Loaded {len(dataset.nodes)} nodes and {len(dataset.edges)} edges."),
            _event("reference_result", f"Returned deterministic reference for policy {policy.value}."),
        ]
        raw = {
            "status": status,
            "primary_node": primary,
            "confidence": confidence,
            "evidence_edges": selected,
            "stability": {"total": confidence, "material_contradiction": any(e.role == "contradicts" for e in dataset.edges)},
            "lyapunov": {"final_energy": energy, "observations": [{"cycle": index} for index in range(cycles + 1)]},
            "receipt": {
                "graphene_executed": False,
                "recorded_reference": True,
                "initial_bundle_hash": digest[:16],
                "final_bundle_hash": digest[16:32],
                "content_hash": digest,
                "recursive_cycles": cycles,
                "visited_states": visited,
                "edges_examined": edges,
                "deepest_hop": min(5, cycles + 1),
            },
            "residual_uncertainty": [
                "Recorded reference output; connect a live GrapheneDB server binary to validate execution."
            ],
        }
        return raw, events


class SubprocessGrapheneDBBackend(Backend):
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        binary = settings.graphenedb_server_binary
        if not binary:
            raise BackendError("GRAPHENEDB_SERVER_BINARY is required for live backend mode")
        self.binary = Path(binary).resolve()
        if not self.binary.exists():
            raise BackendError(f"GrapheneDB server binary not found: {self.binary}")

    @staticmethod
    def _free_port() -> int:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            return int(sock.getsockname()[1])

    @contextmanager
    def _server(self) -> Iterator[tuple[httpx.Client, dict[str, Any]]]:
        work = Path(tempfile.mkdtemp(prefix="graphenedb-evidence-lab-run-"))
        db = work / "db"
        db.mkdir(parents=True)
        port = self._free_port()
        api_key = secrets.token_urlsafe(32)
        log_path = work / "server.log"
        log = open(log_path, "w+", encoding="utf-8")
        env = dict(os.environ)
        env["GRAPHENEDB_API_KEY"] = api_key
        process = subprocess.Popen(
            [
                str(self.binary), str(db), str(self.settings.graphenedb_dimension), str(port),
                "--bind-address", "127.0.0.1",
                "--workers", "2", "--queue-capacity", "32",
                "--rate-limit-rps", "1000", "--rate-limit-burst", "1000",
            ],
            stdout=subprocess.DEVNULL,
            stderr=log,
            env=env,
            text=True,
        )
        client = httpx.Client(
            base_url=f"http://127.0.0.1:{port}",
            headers={"X-API-Key": api_key, "Accept": "application/json"},
            timeout=self.settings.run_timeout_seconds,
        )
        try:
            for _ in range(240):
                if process.poll() is not None:
                    log.flush(); log.seek(0)
                    raise BackendError("GrapheneDB server exited during startup: " + log.read()[-4000:])
                try:
                    response = client.get("/v1/health")
                    if response.status_code == 200:
                        break
                except httpx.HTTPError:
                    pass
                time.sleep(0.025)
            else:
                raise BackendError("GrapheneDB server did not become healthy")
            version_response = client.get("/v1/version")
            version_response.raise_for_status()
            yield client, version_response.json()
        finally:
            client.close()
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            log.close()
            shutil.rmtree(work, ignore_errors=True)

    @staticmethod
    def _node_payload(node: Any) -> dict[str, Any]:
        metadata = dict(node.metadata)
        metadata.update(
            {
                "source_id": node.source_id,
                "evidence_family_id": node.evidence_family_id,
                "derivation_id": node.derivation_id,
            }
        )
        if node.observed_at:
            metadata["observed_at"] = node.observed_at.isoformat()
        if node.valid_from:
            metadata["valid_from"] = node.valid_from.isoformat()
        if node.valid_to:
            metadata["valid_to"] = node.valid_to.isoformat()
        return {
            "external_id": node.external_id,
            "content": node.text,
            "role": node.role.value,
            "metadata": metadata,
        }

    @staticmethod
    def _edge_payload(edge: Any) -> dict[str, Any]:
        metadata = dict(edge.metadata)
        metadata.update(
            {
                "relation": edge.relation,
                "critical": "true" if edge.critical else "false",
                "source_id": edge.source_id,
                "evidence_family_id": edge.evidence_family_id,
                "derivation_id": edge.derivation_id,
                "public_edge_id": edge.edge_id,
            }
        )
        return {
            "from_external_id": edge.from_node,
            "to_external_id": edge.to_node,
            "origin": "observed",
            "role": edge.role.value,
            "confidence": edge.confidence,
            "evidence_id": edge.evidence_id or edge.edge_id,
            "evidence_text": edge.evidence_text,
            "metadata": metadata,
        }

    @staticmethod
    def _policy_payload(policy: PolicyName) -> dict[str, Any]:
        if policy == PolicyName.one_pass:
            return {
                "semantic_candidates": 8,
                "max_hops": 5,
                "max_paths": 32,
                "max_paths_per_root": 8,
                "minimum_confidence": 0.30,
                "reexpansion_threshold": 0.20,
                "max_recursive_cycles": 0,
            }
        if policy == PolicyName.frontier_aware:
            return {
                "semantic_candidates": 8,
                "max_hops": 5,
                "max_paths": 32,
                "max_paths_per_root": 8,
                "minimum_confidence": 0.30,
                "reexpansion_threshold": 0.20,
                "max_recursive_cycles": 3,
            }
        if policy == PolicyName.broad_expansion:
            return {
                "semantic_candidates": 32,
                "max_hops": 8,
                "max_paths": 128,
                "max_paths_per_root": 32,
                "minimum_confidence": 0.20,
                "reexpansion_threshold": 0.05,
                "max_recursive_cycles": 3,
            }
        raise BackendError(
            "previous_stop_reference is a historical benchmark policy and is not executable by the current release binary"
        )

    def run(self, dataset: Dataset, query: Query, policy: PolicyName) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        events: list[dict[str, Any]] = []
        with self._server() as (client, version):
            events.append(_event("worker_started", "Disposable GrapheneDB server became healthy.", version=version))
            node_request = {
                "schema_version": 1,
                "source_id": f"{dataset.manifest.dataset_id}:nodes",
                "signature": 33,
                "nodes": [self._node_payload(node) for node in dataset.nodes],
            }
            response = client.post("/v1/extractions", json=node_request)
            if response.status_code not in {200, 201}:
                raise BackendError(f"node ingestion failed ({response.status_code}): {response.text[:2000]}")
            events.append(_event("nodes_ingested", f"Ingested {len(dataset.nodes)} nodes."))

            edges_by_source: dict[str, list[Any]] = {}
            for edge in dataset.edges:
                edges_by_source.setdefault(edge.source_id, []).append(edge)
            for source_id, edges in sorted(edges_by_source.items()):
                relation_request = {
                    "schema_version": 1,
                    "source_id": f"{dataset.manifest.dataset_id}:edge-source:{source_id}",
                    "signature": 33,
                    "relations": [self._edge_payload(edge) for edge in edges],
                }
                response = client.post("/v1/extractions", json=relation_request)
                if response.status_code not in {200, 201}:
                    raise BackendError(
                        f"edge ingestion failed for {source_id} ({response.status_code}): {response.text[:2000]}"
                    )
            events.append(_event("edges_ingested", f"Ingested {len(dataset.edges)} edges from {len(edges_by_source)} source groups."))

            payload = {
                "query": query.question,
                "signature": 33,
                "mode": query.mode,
                **self._policy_payload(policy),
            }
            events.append(_event("reasoning_started", f"Running policy {policy.value}."))
            response = client.post("/v1/reason/runtime", json=payload)
            if response.status_code != 200:
                raise BackendError(f"reasoning failed ({response.status_code}): {response.text[:4000]}")
            raw = response.json()
            raw["_evidence_lab_version"] = version
            events.append(_event("reasoning_completed", f"GrapheneDB returned status {raw.get('status', 'unknown')}."))
            return raw, events


def create_backend(settings: Settings) -> Backend:
    if settings.backend_mode == "recorded":
        return RecordedReferenceBackend(settings)
    if settings.backend_mode in {"live", "subprocess"}:
        return SubprocessGrapheneDBBackend(settings)
    raise BackendError(f"unsupported EVIDENCE_LAB_BACKEND={settings.backend_mode!r}")
