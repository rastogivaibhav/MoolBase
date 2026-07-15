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

    def get_node(self, node_id: int) -> GrapheneDBResponse:
        return self._request("GET", f"/v1/nodes/{node_id}")

    def search(self, query: str, top_k: int = 5) -> GrapheneDBResponse:
        return self._request("POST", "/v1/search/hybrid", {"query": query, "top_k": top_k})

    def lattice_neighbors(self, node_id: int, hops: int = 2) -> GrapheneDBResponse:
        query = urllib.parse.urlencode({"node_id": node_id, "hops": hops})
        return self._request("GET", f"/v1/search/lattice?{query}")

    def capacity(self) -> GrapheneDBResponse:
        return self._request("GET", "/v1/admin/capacity")

    def validate(self) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/validate", {})

    def checkpoint(self) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/checkpoint", {})

    def backup(self, destination: str) -> GrapheneDBResponse:
        return self._request("POST", "/v1/admin/backup", {"destination": destination})
