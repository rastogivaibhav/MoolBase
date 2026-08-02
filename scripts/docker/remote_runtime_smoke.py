#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import time
import urllib.error
import urllib.request


def request(base_url: str, api_key: str, method: str, path: str, payload=None):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Accept": "application/json", "X-API-Key": api_key}
    if data is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(
        f"{base_url.rstrip('/')}{path}", data=data, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            raw = response.read().decode("utf-8")
            return response.status, json.loads(raw) if raw else None
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8")
        return exc.code, json.loads(raw) if raw else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--api-key", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    token = hashlib.sha256(str(time.time_ns()).encode()).hexdigest()[:12]
    signature = 330000 + int(token[:6], 16) % 1000000
    prefix = f"docker-smoke/{token}"

    status, health = request(args.base_url, args.api_key, "GET", "/v1/health")
    assert status == 200, health

    extraction = {
        "schema_version": 1,
        "source_id": f"{prefix}/incident",
        "signature": signature,
        "nodes": [
            {"external_id": f"{prefix}/root/release", "content": "release changed pool timeout", "role": "root"},
            {"external_id": f"{prefix}/middle/pool", "content": "connection pool exhausted", "role": "node"},
            {"external_id": f"{prefix}/middle/queue", "content": "database wait queue increased", "role": "node"},
            {"external_id": f"{prefix}/symptom/checkout", "content": "checkout failures increased", "role": "symptom"},
            {"external_id": f"{prefix}/root/traffic", "content": "traffic spike alternative", "role": "root"},
        ],
        "relations": [
            {"from_external_id": f"{prefix}/root/release", "to_external_id": f"{prefix}/middle/pool", "origin": "observed", "role": "mechanistic", "confidence": 0.97, "evidence_id": f"{prefix}/release-log", "evidence_text": "timeout configuration changed"},
            {"from_external_id": f"{prefix}/middle/pool", "to_external_id": f"{prefix}/symptom/checkout", "origin": "discovered", "role": "causal", "confidence": 0.96, "evidence_id": f"{prefix}/heap-profile", "evidence_text": "pool exhaustion preceded failures"},
            {"from_external_id": f"{prefix}/root/release", "to_external_id": f"{prefix}/middle/queue", "origin": "observed", "role": "mechanistic", "confidence": 0.92, "evidence_id": f"{prefix}/config-diff", "evidence_text": "database wait configuration changed"},
            {"from_external_id": f"{prefix}/middle/queue", "to_external_id": f"{prefix}/symptom/checkout", "origin": "observed", "role": "supports", "confidence": 0.90, "evidence_id": f"{prefix}/database-metrics", "evidence_text": "wait queue correlated with failures"},
            {"from_external_id": f"{prefix}/root/traffic", "to_external_id": f"{prefix}/symptom/checkout", "origin": "observed", "role": "causal", "confidence": 0.55, "evidence_id": f"{prefix}/traffic-dashboard", "evidence_text": "traffic increased in the same window"},
        ],
    }
    status, imported = request(args.base_url, args.api_key, "POST", "/v1/extractions", extraction)
    assert status == 201, imported

    payload = {
        "query": "checkout failures increased",
        "signature": signature,
        "mode": "empirical",
        "semantic_candidates": 8,
        "max_hops": 5,
        "max_paths": 32,
        "max_paths_per_root": 8,
        "minimum_confidence": 0.30,
        "reexpansion_threshold": 0.20,
        "max_recursive_cycles": 2,
    }
    status, first = request(args.base_url, args.api_key, "POST", "/v1/reason/runtime", payload)
    assert status == 200, first
    assert first["status"] not in {"abstain", "evidence_required"}
    receipt = first["receipt"]
    for field in (
        "graphene_executed",
        "fiber_bundle_built",
        "stability_critic_executed",
        "lyapunov_trajectory_executed",
        "convergence_executed",
        "opposition_executed",
        "governed_projection_executed",
        "no_silent_promotion",
    ):
        assert receipt[field] is True, (field, receipt)
    assert receipt["initial_bundle_hash"] != 0
    assert receipt["final_bundle_hash"] != 0
    assert len(first["evidence_edges"]) >= 2

    status, repeated = request(args.base_url, args.api_key, "POST", "/v1/reason/runtime", payload)
    assert status == 200, repeated
    assert repeated["status"] == first["status"]
    assert repeated["primary_node"] == first["primary_node"]
    assert repeated["receipt"]["initial_bundle_hash"] == receipt["initial_bundle_hash"]
    assert repeated["receipt"]["final_bundle_hash"] == receipt["final_bundle_hash"]
    assert repeated["lyapunov"]["final_energy"] == first["lyapunov"]["final_energy"]

    result = {
        "api_smoke": True,
        "base_url": args.base_url,
        "signature": signature,
        "status": first["status"],
        "primary_node": first["primary_node"],
        "evidence_edges": len(first["evidence_edges"]),
        "initial_bundle_hash": receipt["initial_bundle_hash"],
        "final_bundle_hash": receipt["final_bundle_hash"],
        "lyapunov_final_energy": first["lyapunov"]["final_energy"],
        "deterministic_repeat": True,
    }
    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
