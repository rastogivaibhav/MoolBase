"""Dependency-free GrapheneDB HTTP client for the v1 pilot API."""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Mapping, Optional


class GrapheneDBError(RuntimeError):
    """Raised when GrapheneDB returns an error response or invalid JSON."""

    def __init__(self, status: int, message: str, payload: Any = None) -> None:
        super().__init__(f"GrapheneDB HTTP {status}: {message}")
        self.status = status
        self.payload = payload


@dataclass(frozen=True)
class GrapheneDBResponse:
    status: int
    data: Any
    request_id: Optional[str]
    server_version: Optional[str]
    api_version: Optional[str]


class GrapheneDBClient:
    """Small synchronous client for controlled GrapheneDB pilots.

    The client deliberately uses only the Python standard library so it can be
    copied into incident tooling, migration scripts, and air-gapped pilots.
    """

    def __init__(
        self,
        base_url: str = "http://127.0.0.1:8080",
        api_key: Optional[str] = None,
        timeout: float = 10.0,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.api_key = api_key if api_key is not None else os.getenv("GRAPHENEDB_API_KEY")
        self.timeout = timeout

    def _request(
        self,
        method: str,
        path: str,
        payload: Optional[Mapping[str, Any]] = None,
        *,
        idempotency_key: Optional[str] = None,
        authenticated: bool = True,
    ) -> GrapheneDBResponse:
        body = None if payload is None else json.dumps(payload, separators=(",", ":")).encode("utf-8")
        headers = {"Accept": "application/json"}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if authenticated and self.api_key:
            headers["X-API-Key"] = self.api_key
        if idempotency_key:
            headers["Idempotency-Key"] = idempotency_key
        request = urllib.request.Request(self.base_url + path, data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                raw = response.read()
                data = self._decode_json(raw)
                return GrapheneDBResponse(
                    status=response.status,
                    data=data,
                    request_id=response.headers.get("X-Request-ID"),
                    server_version=response.headers.get("X-GrapheneDB-Version"),
                    api_version=response.headers.get("X-GrapheneDB-API-Version"),
                )
        except urllib.error.HTTPError as exc:
            raw = exc.read()
            payload_data = self._decode_json(raw, tolerate_invalid=True)
            if isinstance(payload_data, dict):
                message = str(payload_data.get("error") or payload_data.get("message") or exc.reason)
            else:
                message = str(exc.reason)
            raise GrapheneDBError(exc.code, message, payload_data) from exc
        except urllib.error.URLError as exc:
            raise GrapheneDBError(0, str(exc.reason)) from exc

    @staticmethod
    def _decode_json(raw: bytes, tolerate_invalid: bool = False) -> Any:
        if not raw:
            return None
        try:
            return json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            if tolerate_invalid:
                return raw.decode("utf-8", errors="replace")
            raise GrapheneDBError(0, "invalid JSON response") from exc

    def health(self) -> GrapheneDBResponse:
        return self._request("GET", "/v1/health", authenticated=False)

    def ready(self) -> GrapheneDBResponse:
        return self._request("GET", "/v1/ready", authenticated=False)

    def version(self) -> GrapheneDBResponse:
        return self._request("GET", "/v1/version", authenticated=False)

    def put_node(
        self,
        text: str,
        *,
        source: str = "python-client",
        metadata: Optional[Mapping[str, Any]] = None,
        placement_policy: Optional[str] = None,
        idempotency_key: Optional[str] = None,
    ) -> GrapheneDBResponse:
        payload: dict[str, Any] = {"text": text, "source": source}
        if metadata:
            payload.update(metadata)
        if placement_policy:
            payload["placement_policy"] = placement_policy
        return self._request("POST", "/v1/nodes", payload, idempotency_key=idempotency_key)

    def put_fact(
        self,
        text: str,
        *,
        source: str = "python-client",
        metadata: Optional[Mapping[str, Any]] = None,
        idempotency_key: Optional[str] = None,
    ) -> GrapheneDBResponse:
        payload: dict[str, Any] = {"text": text, "source": source}
        if metadata:
            payload.update(metadata)
        return self._request("POST", "/v1/facts", payload, idempotency_key=idempotency_key)

    def put_extraction(
        self, extraction: Mapping[str, Any]
    ) -> GrapheneDBResponse:
        """Atomically ingest a versioned causal extraction.

        Node vectors are generated by the controlled-pilot server. Stable
        ``source_id`` plus node ``external_id`` values provide durable replay
        safety; a changed replay is rejected with HTTP 409.
        """
        return self._request("POST", "/v1/extractions", extraction)

    def get_node(self, node_id: int) -> GrapheneDBResponse:
        return self._request("GET", f"/v1/nodes/{node_id}")

    def search(self, query: str, top_k: int = 5) -> GrapheneDBResponse:
        return self._request("POST", "/v1/search/hybrid", {"query": query, "top_k": top_k})

    def lattice_neighbors(self, node_id: int, hops: int = 2) -> GrapheneDBResponse:
        query = urllib.parse.urlencode({"node_id": node_id, "hops": hops})
        return self._request("GET", f"/v1/search/lattice?{query}")

    def reason_dialectic(
        self,
        query: str,
        *,
        signature: int = 0,
        mode: str = "balanced",
        as_of: Optional[str] = None,
        semantic_candidates: int = 12,
        max_hops: int = 6,
        max_paths: int = 32,
        max_paths_per_root: int = 8,
        max_visited_states: int = 20000,
        max_opposition_rounds: int = 1,
        minimum_confidence: float = 0.45,
        reexpansion_threshold: float = 0.25,
    ) -> GrapheneDBResponse:
        payload: dict[str, Any] = {
            "query": query,
            "signature": signature,
            "mode": mode,
            "semantic_candidates": semantic_candidates,
            "max_hops": max_hops,
            "max_paths": max_paths,
            "max_paths_per_root": max_paths_per_root,
            "max_visited_states": max_visited_states,
            "max_opposition_rounds": max_opposition_rounds,
            "minimum_confidence": minimum_confidence,
            "reexpansion_threshold": reexpansion_threshold,
        }
        if as_of is not None:
            payload["as_of"] = as_of
        return self._request("POST", "/v1/reason/dialectic", payload)

    def reason_hypokosh(
        self,
        query: str,
        *,
        tenant_id: Optional[str] = None,
        use_active_policy: bool = False,
        signature: int = 0,
        max_hypotheses: int = 8,
        mode: str = "balanced",
        as_of: Optional[str] = None,
    ) -> GrapheneDBResponse:
        """Generate read-only, explicitly hypothetical causal proposals."""
        payload: dict[str, Any] = {
            "query": query,
            "signature": signature,
            "max_hypotheses": max_hypotheses,
            "mode": mode,
            "use_active_policy": use_active_policy,
        }
        if tenant_id is not None:
            payload["tenant_id"] = tenant_id
        if as_of is not None:
            payload["as_of"] = as_of
        return self._request("POST", "/v1/reason/hypokosh", payload)

    def record_learning_episode(
        self, episode: Mapping[str, Any]
    ) -> GrapheneDBResponse:
        """Record one immutable, replay-safe governed learning episode."""
        return self._request("POST", "/v1/learning/episodes", episode)

    def evaluate_learning_policies(
        self,
        tenant_id: str,
        baseline: Mapping[str, Any],
        *,
        options: Optional[Mapping[str, Any]] = None,
    ) -> GrapheneDBResponse:
        """Evaluate retrieval policies without activating or writing one."""
        payload: dict[str, Any] = {
            "tenant_id": tenant_id,
            "baseline": dict(baseline),
        }
        if options is not None:
            payload["options"] = dict(options)
        return self._request(
            "POST", "/v1/learning/policies/evaluate", payload
        )

    def decide_learning_policy(
        self, decision: Mapping[str, Any]
    ) -> GrapheneDBResponse:
        """Append an explicitly approved promotion or rollback decision."""
        return self._request(
            "POST", "/v1/learning/policies/decisions", decision
        )

    def current_learning_policy(
        self, tenant_id: str
    ) -> GrapheneDBResponse:
        query = urllib.parse.urlencode({"tenant_id": tenant_id})
        return self._request(
            "GET", f"/v1/learning/policies/current?{query}"
        )

    def quarantine_learning_episode(
        self, tenant_id: str, episode_id: str
    ) -> GrapheneDBResponse:
        return self._request(
            "POST",
            "/v1/learning/episodes/quarantine",
            {"tenant_id": tenant_id, "episode_id": episode_id},
        )

    def capacity(self) -> GrapheneDBResponse:
        return self._request("GET", "/v1/admin/capacity")

    def validate(self) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/validate", {})

    def validate_provenance(self) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/validate/provenance", {})

    def checkpoint(self) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/checkpoint", {})

    def backup(self, destination: str) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/backup", {"destination": destination})
