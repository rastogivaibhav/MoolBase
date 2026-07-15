#!/usr/bin/env python3
"""Pilot API/lifecycle contract test for GrapheneDB 0.6.0-rc1."""

from __future__ import annotations

import json
import os
import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
BINARY = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "graphenedb_server").resolve()
CLIENT_DIR = ROOT / "clients" / "python"
sys.path.insert(0, str(CLIENT_DIR))
from graphenedb_client import GrapheneDBClient, GrapheneDBError  # noqa: E402

API_KEY = "pilot-contract-key"
WORK = pathlib.Path(tempfile.mkdtemp(prefix="graphenedb-pilot-contract-"))


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def http_request(port: int, method: str, path: str, payload=None, headers=None):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    request_headers = {"Accept": "application/json"}
    if data is not None:
        request_headers["Content-Type"] = "application/json"
    request_headers["X-API-Key"] = API_KEY
    if headers:
        request_headers.update(headers)
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}{path}", data=data, headers=request_headers, method=method
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            raw = response.read().decode("utf-8")
            return response.status, json.loads(raw) if raw else None, dict(response.headers)
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8")
        return exc.code, json.loads(raw) if raw else None, dict(exc.headers)


def raw_request(port: int, payload: bytes) -> str:
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.sendall(payload)
        sock.shutdown(socket.SHUT_WR)
        chunks = []
        while True:
            part = sock.recv(65536)
            if not part:
                break
            chunks.append(part)
    return b"".join(chunks).decode("utf-8", errors="replace")


def start_server(db_dir: pathlib.Path, port: int, radius: int = 10, max_bulk: int = 50):
    db_dir.mkdir(parents=True, exist_ok=True)
    log_path = db_dir.parent / f"{db_dir.name}.server.jsonl"
    log = open(log_path, "w+", encoding="utf-8")
    env = dict(os.environ)
    env["GRAPHENEDB_API_KEY"] = API_KEY
    command = [
        str(BINARY), str(db_dir), "16", str(port),
        "--physical-lattice-primary", "--physical-lattice-radius", str(radius),
        "--expected-max-nodes", str(1 + 3 * radius * (radius + 1)),
        "--workers", "4", "--queue-capacity", "64",
        "--rate-limit-rps", "10000", "--rate-limit-burst", "10000",
        "--max-request-bytes", str(1024 * 1024),
        "--max-bulk-nodes", str(max_bulk),
        "--socket-timeout-seconds", "3",
    ]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=log, text=True, env=env)
    for _ in range(200):
        if process.poll() is not None:
            log.flush(); log.seek(0)
            raise RuntimeError(f"server exited during startup: {log.read()}")
        try:
            status, _, _ = http_request(port, "GET", "/v1/health")
            if status == 200:
                return process, log, log_path
        except Exception:
            pass
        time.sleep(0.025)
    process.kill()
    raise RuntimeError("server did not become healthy")


def stop_server(process: subprocess.Popen, log) -> str:
    process.send_signal(signal.SIGTERM)
    process.wait(timeout=30)
    log.flush(); log.seek(0)
    text = log.read()
    log.close()
    if process.returncode != 0:
        raise RuntimeError(f"graceful shutdown returned {process.returncode}: {text}")
    return text


def node_count(port: int) -> int:
    status, data, _ = http_request(port, "GET", "/v1/metrics")
    assert status == 200
    return int(data["node_count"])


results: dict[str, object] = {}
try:
    db_dir = WORK / "main"
    port = free_port()
    process, log, _ = start_server(db_dir, port)

    # Public version discovery and response contract.
    req = urllib.request.Request(f"http://127.0.0.1:{port}/v1/version", method="GET")
    with urllib.request.urlopen(req, timeout=5) as response:
        version = json.loads(response.read())
        results["version_discovery"] = (
            response.status == 200
            and version["server_version"] == "0.6.0-rc1"
            and version["api_version"] == 1
            and response.headers.get("X-GrapheneDB-API-Version") == "1"
            and "idempotent_node_writes" in version["features"]
        )

    # Dependency-free client and durable idempotency contract.
    client = GrapheneDBClient(f"http://127.0.0.1:{port}", api_key=API_KEY)
    created = client.put_node("retry-safe node", idempotency_key="retry-safe-1")
    replay = client.put_node("retry-safe node", idempotency_key="retry-safe-1")
    results["python_client"] = created.status == 201 and client.get_node(created.data["id"]).data["content"] == "retry-safe node"
    results["idempotent_replay"] = (
        replay.status == 200
        and replay.data["id"] == created.data["id"]
        and replay.data["idempotent_replay"] is True
        and node_count(port) == 1
    )
    try:
        client.put_node("different content", idempotency_key="retry-safe-1")
        conflict = False
    except GrapheneDBError as exc:
        conflict = exc.status == 409
    results["idempotency_conflict"] = conflict and node_count(port) == 1

    # Strict HTTP framing rejects ambiguous or unsupported requests.
    no_length = raw_request(port, (
        "POST /v1/nodes HTTP/1.1\r\nHost: localhost\r\n"
        f"X-API-Key: {API_KEY}\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"
    ).encode())
    results["length_required"] = no_length.startswith("HTTP/1.1 411")

    chunked = raw_request(port, (
        "POST /v1/nodes HTTP/1.1\r\nHost: localhost\r\n"
        f"X-API-Key: {API_KEY}\r\nContent-Type: application/json\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n0\r\n\r\n"
    ).encode())
    results["chunked_rejected"] = chunked.startswith("HTTP/1.1 501")

    wrong_type_body = b'{"text":"wrong type"}'
    wrong_type = raw_request(port, (
        "POST /v1/nodes HTTP/1.1\r\nHost: localhost\r\n"
        f"X-API-Key: {API_KEY}\r\nContent-Type: text/plain\r\n"
        f"Content-Length: {len(wrong_type_body)}\r\nConnection: close\r\n\r\n"
    ).encode() + wrong_type_body)
    results["content_type_required"] = wrong_type.startswith("HTTP/1.1 415")

    conflicting_length = raw_request(port, (
        "POST /v1/nodes HTTP/1.1\r\nHost: localhost\r\n"
        f"X-API-Key: {API_KEY}\r\nContent-Type: application/json\r\n"
        "Content-Length: 2\r\nContent-Length: 3\r\nConnection: close\r\n\r\n{}"
    ).encode())
    results["conflicting_content_length_rejected"] = conflicting_length.startswith("HTTP/1.1 400")

    # Atomic bulk write and bounded admission.
    before_bulk = node_count(port)
    status, bulk, _ = http_request(port, "POST", "/v1/nodes/bulk", {"count": 20, "prefix": "pilot bulk"})
    results["atomic_bulk"] = status == 201 and bulk["inserted"] == 20 and bulk["atomic"] is True and node_count(port) == before_bulk + 20
    before_reject = node_count(port)
    status, rejected, _ = http_request(port, "POST", "/v1/nodes/bulk", {"count": 51})
    results["bulk_bound"] = status == 400 and rejected["maximum"] == 50 and node_count(port) == before_reject
    before_prefix_reject = node_count(port)
    status, rejected, _ = http_request(port, "POST", "/v1/nodes/bulk", {"count": 2, "prefix": "x" * 4097})
    results["bulk_amplification_guard"] = status == 400 and rejected["error"] == "bulk_prefix_too_large" and node_count(port) == before_prefix_reject

    # Invalid client inputs map to bounded 4xx responses instead of internal errors.
    status, edge_error, _ = http_request(port, "POST", "/v1/edges", {"from": 999999, "to": 999998})
    results["invalid_edge_is_400"] = status == 400 and "error" in edge_error
    status, invalid_node, _ = http_request(port, "GET", "/v1/nodes/not-a-number")
    results["invalid_node_id_is_400"] = status == 400 and invalid_node["error"] == "invalid_node_id"
    status, lattice, _ = http_request(port, "GET", f"/v1/search/lattice?node_id={created.data['id']}&hops=999")
    results["lattice_hops_bounded"] = status == 200 and lattice["hops"] == 16

    for index in range(4):
        client.put_fact(
            f"temporal fact {index}",
            metadata={"valid_from": "2026-01-01", "valid_until": "2027-01-01"},
            idempotency_key=f"temporal-{index}",
        )
    status, temporal, _ = http_request(port, "POST", "/v1/retrieve/temporal", {"as_of": "2026-06-01", "limit": 2})
    results["temporal_results_bounded"] = status == 200 and temporal["returned"] == 2 and len(temporal["results"]) == 2

    # SIGTERM drains workers, checkpoints, and leaves a zero-byte WAL.
    durable_id = client.put_node("survive graceful shutdown", idempotency_key="shutdown-node").data["id"]
    logs = stop_server(process, log)
    parsed_logs = [json.loads(line) for line in logs.splitlines() if line.strip()]
    events = {entry.get("event") for entry in parsed_logs}
    results["graceful_shutdown_events"] = {"shutdown_started", "shutdown_checkpoint", "shutdown_complete"}.issubset(events)
    wal_path = db_dir / "graphene.wal"
    results["shutdown_checkpoint_zero_wal"] = wal_path.exists() and wal_path.stat().st_size == 0

    process, log, _ = start_server(db_dir, port)
    client = GrapheneDBClient(f"http://127.0.0.1:{port}", api_key=API_KEY)
    results["restart_after_graceful_shutdown"] = client.get_node(durable_id).data["content"] == "survive graceful shutdown"
    results["post_restart_validation"] = client.validate().data["ok"] is True
    stop_server(process, log)

    results["ok"] = all(bool(value) for value in results.values())
    print(json.dumps(results, indent=2, sort_keys=True))
    raise SystemExit(0 if results["ok"] else 1)
finally:
    shutil.rmtree(WORK, ignore_errors=True)
