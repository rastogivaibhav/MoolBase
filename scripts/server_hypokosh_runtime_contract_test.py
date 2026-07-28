#!/usr/bin/env python3
"""Focused contract for the complete HypoKosh runtime endpoint."""

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

BINARY = pathlib.Path(sys.argv[1]).resolve()
API_KEY = "hypokosh-runtime-contract"
WORK = pathlib.Path(tempfile.mkdtemp(prefix="graphenedb-hypokosh-runtime-"))


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def request(port: int, method: str, path: str, payload=None):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Accept": "application/json", "X-API-Key": API_KEY}
    if data is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}{path}", data=data, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            raw = response.read().decode("utf-8")
            return response.status, json.loads(raw) if raw else None
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8")
        return exc.code, json.loads(raw) if raw else None


def start_server(port: int):
    db = WORK / "db"
    db.mkdir(parents=True)
    log = open(WORK / "server.log", "w+", encoding="utf-8")
    env = dict(os.environ)
    env["GRAPHENEDB_API_KEY"] = API_KEY
    process = subprocess.Popen(
        [
            str(BINARY), str(db), "16", str(port),
            "--bind-address", "127.0.0.1",
            "--workers", "2", "--queue-capacity", "32",
            "--rate-limit-rps", "1000", "--rate-limit-burst", "1000",
        ],
        stdout=subprocess.DEVNULL,
        stderr=log,
        env=env,
        text=True,
    )
    for _ in range(200):
        if process.poll() is not None:
            log.flush(); log.seek(0)
            raise RuntimeError(log.read())
        try:
            status, _ = request(port, "GET", "/v1/health")
            if status == 200:
                return process, log
        except Exception:
            pass
        time.sleep(0.025)
    process.kill()
    raise RuntimeError("server did not become healthy")


def stop_server(process, log):
    process.send_signal(signal.SIGTERM)
    process.wait(timeout=30)
    log.close()
    if process.returncode != 0:
        raise RuntimeError(f"server exited with {process.returncode}")


try:
    port = free_port()
    process, log = start_server(port)
    extraction = {
        "schema_version": 1,
        "source_id": "runtime-contract-incident",
        "signature": 33,
        "nodes": [
            {"external_id": "root/release", "content": "release changed pool timeout", "role": "root"},
            {"external_id": "middle/pool", "content": "connection pool exhausted", "role": "node"},
            {"external_id": "middle/queue", "content": "database wait queue increased", "role": "node"},
            {"external_id": "symptom/checkout", "content": "checkout failures increased", "role": "symptom"},
            {"external_id": "root/traffic", "content": "traffic spike alternative", "role": "root"},
        ],
        "relations": [
            {
                "from_external_id": "root/release",
                "to_external_id": "middle/pool",
                "origin": "observed",
                "role": "mechanistic",
                "confidence": 0.97,
                "evidence_id": "release-log",
                "evidence_text": "timeout configuration changed",
            },
            {
                "from_external_id": "middle/pool",
                "to_external_id": "symptom/checkout",
                "origin": "discovered",
                "role": "causal",
                "confidence": 0.96,
                "evidence_id": "heap-profile",
                "evidence_text": "pool exhaustion preceded failures",
            },
            {
                "from_external_id": "root/release",
                "to_external_id": "middle/queue",
                "origin": "observed",
                "role": "mechanistic",
                "confidence": 0.92,
                "evidence_id": "config-diff",
                "evidence_text": "database wait configuration changed",
            },
            {
                "from_external_id": "middle/queue",
                "to_external_id": "symptom/checkout",
                "origin": "observed",
                "role": "supports",
                "confidence": 0.90,
                "evidence_id": "database-metrics",
                "evidence_text": "wait queue correlated with failures",
            },
            {
                "from_external_id": "root/traffic",
                "to_external_id": "symptom/checkout",
                "origin": "observed",
                "role": "causal",
                "confidence": 0.55,
                "evidence_id": "traffic-dashboard",
                "evidence_text": "traffic increased in the same window",
            },
        ],
    }
    status, imported = request(port, "POST", "/v1/extractions", extraction)
    assert status == 201, imported

    status, metrics_before = request(port, "GET", "/v1/metrics")
    assert status == 200
    payload = {
        "query": "checkout failures increased",
        "signature": 33,
        "mode": "empirical",
        "semantic_candidates": 8,
        "max_hops": 5,
        "max_paths": 32,
        "max_paths_per_root": 8,
        "minimum_confidence": 0.30,
        "reexpansion_threshold": 0.20,
        "max_recursive_cycles": 2,
    }
    status, first = request(port, "POST", "/v1/reason/runtime", payload)
    assert status == 200, first
    assert first["status"] not in {"abstain", "evidence_required"}
    assert first["receipt"]["graphene_executed"] is True
    assert first["receipt"]["fiber_bundle_built"] is True
    assert first["receipt"]["stability_critic_executed"] is True
    assert first["receipt"]["escape_considered"] is True
    assert first["receipt"]["convergence_executed"] is True
    assert first["receipt"]["opposition_executed"] is True
    assert first["receipt"]["governed_projection_executed"] is True
    assert first["receipt"]["no_silent_promotion"] is True
    assert first["receipt"]["initial_bundle_hash"] != 0
    assert first["receipt"]["final_bundle_hash"] != 0
    assert len(first["evidence_edges"]) >= 2
    assert 0.0 <= first["stability"]["total"] <= 1.0

    status, repeated = request(port, "POST", "/v1/reason/runtime", payload)
    assert status == 200
    assert repeated["status"] == first["status"]
    assert repeated["primary_node"] == first["primary_node"]
    assert repeated["receipt"]["initial_bundle_hash"] == first["receipt"]["initial_bundle_hash"]
    assert repeated["receipt"]["final_bundle_hash"] == first["receipt"]["final_bundle_hash"]

    status, metrics_after = request(port, "GET", "/v1/metrics")
    assert status == 200
    assert metrics_after["node_count"] == metrics_before["node_count"]
    assert metrics_after["edge_count"] == metrics_before["edge_count"]

    stop_server(process, log)
    print(json.dumps({
        "hypokosh_runtime_contract": True,
        "status": first["status"],
        "initial_bundle_hash": first["receipt"]["initial_bundle_hash"],
        "final_bundle_hash": first["receipt"]["final_bundle_hash"],
        "evidence_edges": len(first["evidence_edges"]),
    }, sort_keys=True))
finally:
    shutil.rmtree(WORK, ignore_errors=True)
