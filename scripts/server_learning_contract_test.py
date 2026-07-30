#!/usr/bin/env python3
"""End-to-end governed HypoKosh/outcome-learning API contract."""

from __future__ import annotations

import json
import os
import pathlib
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
BINARY = pathlib.Path(
    sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "graphenedb_server"
).resolve()
sys.path.insert(0, str(ROOT / "clients" / "python"))
from graphenedb_client import GrapheneDBClient, GrapheneDBError  # noqa: E402

API_KEY = "governed-learning-contract-key"
WORK = pathlib.Path(tempfile.mkdtemp(prefix="graphenedb-learning-contract-"))


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def start_server(db_dir: pathlib.Path, port: int):
    db_dir.mkdir(parents=True, exist_ok=True)
    log = open(WORK / f"server-{port}.jsonl", "w+", encoding="utf-8")
    environment = dict(os.environ)
    environment["GRAPHENEDB_API_KEY"] = API_KEY
    command = [
        str(BINARY),
        str(db_dir),
        "16",
        str(port),
        "--physical-lattice-primary",
        "--physical-lattice-radius",
        "20",
        "--expected-max-nodes",
        "1261",
        "--workers",
        "4",
        "--queue-capacity",
        "64",
        "--rate-limit-rps",
        "10000",
        "--rate-limit-burst",
        "10000",
        "--max-request-bytes",
        str(1024 * 1024),
        "--socket-timeout-seconds",
        "3",
    ]
    process = subprocess.Popen(
        command,
        stdout=subprocess.DEVNULL,
        stderr=log,
        text=True,
        env=environment,
    )
    client = GrapheneDBClient(
        f"http://127.0.0.1:{port}", api_key=API_KEY
    )
    for _ in range(200):
        if process.poll() is not None:
            log.flush()
            log.seek(0)
            raise RuntimeError(f"server exited during startup: {log.read()}")
        try:
            if client.health().status == 200:
                return process, log, client
        except GrapheneDBError:
            pass
        time.sleep(0.025)
    process.kill()
    raise RuntimeError("server did not become healthy")


def stop_server(process: subprocess.Popen, log) -> None:
    process.send_signal(signal.SIGTERM)
    process.wait(timeout=30)
    log.flush()
    log.seek(0)
    text = log.read()
    log.close()
    if process.returncode != 0:
        raise RuntimeError(
            f"graceful shutdown returned {process.returncode}: {text}"
        )


def metrics(client: GrapheneDBClient) -> dict:
    return client._request("GET", "/v1/metrics").data


def episode(
    episode_id: str,
    split: str,
    policy: dict,
    *,
    success: bool,
    quality: float,
    evidence_nodes: list[int],
) -> dict:
    return {
        "schema_version": 1,
        "tenant_id": "merchant-risk",
        "episode_id": episode_id,
        "family": "incident-diagnosis",
        "domain": "checkout",
        "split": split,
        "query": "Why did checkout latency increase?",
        "signature": 33,
        "model_version": "reasoner-1",
        "policy": policy,
        "outcome_verified": True,
        "verifier_id": "postmortem-reviewer",
        "outcome_evidence_id": "postmortem-42",
        "task_success": success,
        "causal_f1": quality,
        "evidence_coverage": quality,
        "calibration_error": 1.0 - quality,
        "latency_ms": 25,
        "token_cost": 0.2,
        "action_cost": 0.1,
        "intermediate_trace_bytes": 1000,
        "retained_trace_bytes": 200 if success else 600,
        "decisive_evidence_total": 2,
        "decisive_evidence_retained": 2 if success else 1,
        "useful_evidence_total": 4,
        "useful_evidence_retrieved_at_20": 4 if success else 2,
        "evidence_node_ids": evidence_nodes,
    }


def policy_decision(event_id: str, action: str, policy: dict) -> dict:
    return {
        "schema_version": 1,
        "tenant_id": "merchant-risk",
        "event_id": event_id,
        "action": action,
        "policy": policy,
        "approved": True,
        "approver_id": "memory-governance-board",
        "evaluation_reference": "eval://GDB-GL-0/api-run-1",
        "reason": "passed governed development and safety gates",
    }


db_dir = WORK / "db"
port = free_port()
process, log, client = start_server(db_dir, port)
try:
    extraction = client.put_extraction(
        {
            "schema_version": 1,
            "source_id": "learning-contract-evidence",
            "source_uri": "https://evidence.example/postmortem-42",
            "extraction_run_id": "extractor-1",
            "signature": 33,
            "nodes": [
                {
                    "external_id": "root/pool",
                    "content": "connection pool exhaustion caused checkout latency",
                    "role": "root",
                },
                {
                    "external_id": "symptom/latency",
                    "content": "checkout latency increased",
                    "role": "symptom",
                },
            ],
            "relations": [
                {
                    "from_external_id": "root/pool",
                    "to_external_id": "symptom/latency",
                    "origin": "observed",
                    "role": "causal",
                    "confidence": 0.95,
                    "evidence_id": "postmortem-42",
                    "evidence_text": "pool wait preceded latency",
                }
            ],
        }
    )
    evidence_nodes = list(extraction.data["external_to_node_id"].values())

    nodes_before_hypokosh = metrics(client)["node_count"]
    hypotheses = client.reason_hypokosh(
        "checkout latency increased", signature=33, max_hypotheses=4
    )
    assert hypotheses.status == 200
    assert hypotheses.data["durable_writes"] is False
    assert hypotheses.data["proposals"]
    assert all(
        proposal["origin"] == "hypothetical"
        and proposal["eligible_for_truth_promotion"] is False
        for proposal in hypotheses.data["proposals"]
    )
    assert metrics(client)["node_count"] == nodes_before_hypokosh

    baseline = {
        "version": "baseline-v1",
        "semantic_candidates": 8,
        "max_hops": 6,
        "max_paths": 32,
        "max_paths_per_root": 8,
        "max_visited_states": 20000,
        "max_opposition_rounds": 1,
        "minimum_confidence": 0.55,
        "reexpansion_threshold": 0.25,
    }
    candidate = dict(baseline)
    candidate.update(
        {
            "version": "candidate-v2",
            "semantic_candidates": 16,
            "minimum_confidence": 0.45,
        }
    )
    episodes = [
        episode(
            "baseline-train-1",
            "training",
            baseline,
            success=False,
            quality=0.45,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "baseline-train-2",
            "training",
            baseline,
            success=True,
            quality=0.60,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "baseline-dev-1",
            "development",
            baseline,
            success=False,
            quality=0.45,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "candidate-train-1",
            "training",
            candidate,
            success=True,
            quality=0.92,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "candidate-train-2",
            "training",
            candidate,
            success=True,
            quality=0.95,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "candidate-dev-1",
            "development",
            candidate,
            success=True,
            quality=0.96,
            evidence_nodes=evidence_nodes,
        ),
        episode(
            "candidate-eval-1",
            "evaluation",
            candidate,
            success=True,
            quality=1.0,
            evidence_nodes=evidence_nodes,
        ),
    ]
    created = [client.record_learning_episode(value) for value in episodes]
    assert all(response.status == 201 for response in created)
    replay = client.record_learning_episode(episodes[0])
    assert replay.status == 200 and replay.data["idempotent_replay"] is True
    changed = json.loads(json.dumps(episodes[0]))
    changed["task_success"] = True
    try:
        client.record_learning_episode(changed)
        raise AssertionError("changed learning episode replay was accepted")
    except GrapheneDBError as error:
        assert error.status == 409

    nodes_before_evaluation = metrics(client)["node_count"]
    evaluation = client.evaluate_learning_policies(
        "merchant-risk", baseline
    )
    assert evaluation.status == 200
    assert evaluation.data["durable_writes"] is False
    assert evaluation.data["has_recommendation"] is True
    assert evaluation.data["recommended"]["version"] == "candidate-v2"
    assert evaluation.data["evaluation_episodes_excluded"] == 1
    assert metrics(client)["node_count"] == nodes_before_evaluation

    unapproved = policy_decision("unapproved", "promote", candidate)
    unapproved["approved"] = False
    try:
        client.decide_learning_policy(unapproved)
        raise AssertionError("unapproved policy decision was accepted")
    except GrapheneDBError as error:
        assert error.status == 400

    client.decide_learning_policy(
        policy_decision("promote-baseline", "promote", baseline)
    )
    promoted = client.decide_learning_policy(
        policy_decision("promote-candidate", "promote", candidate)
    )
    assert promoted.status == 201
    assert (
        client.current_learning_policy("merchant-risk")
        .data["policy"]["version"]
        == "candidate-v2"
    )

    active_hypotheses = client.reason_hypokosh(
        "checkout latency increased",
        tenant_id="merchant-risk",
        use_active_policy=True,
        signature=33,
    )
    assert active_hypotheses.data["active_policy"]["version"] == "candidate-v2"
    assert active_hypotheses.data["durable_writes"] is False

    rolled_back = client.decide_learning_policy(
        policy_decision("rollback-baseline", "rollback", baseline)
    )
    assert rolled_back.status == 201
    assert rolled_back.data["action"] == "rollback"
    assert (
        client.current_learning_policy("merchant-risk")
        .data["policy"]["version"]
        == "baseline-v1"
    )

    try:
        client._request(
            "POST",
            "/v1/learning/episodes/quarantine",
            {
                "tenant_id": "merchant-risk",
                "episode_id": "baseline-train-1",
                "unexpected": True,
            },
        )
        raise AssertionError("unknown JSON field was accepted")
    except GrapheneDBError as error:
        assert error.status == 400
finally:
    stop_server(process, log)

port = free_port()
process, log, client = start_server(db_dir, port)
try:
    current = client.current_learning_policy("merchant-risk")
    assert current.data["policy"]["version"] == "baseline-v1"
    replay_after_restart = client.record_learning_episode(episodes[0])
    assert replay_after_restart.status == 200
    assert replay_after_restart.data["idempotent_replay"] is True
    assert "read_only_hypokosh" in client.version().data["features"]
    print(
        "server_governed_learning_contract_passed=true "
        "evaluation_excluded=1 active_after_restart=baseline-v1"
    )
finally:
    stop_server(process, log)
