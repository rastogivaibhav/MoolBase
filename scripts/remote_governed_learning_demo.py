#!/usr/bin/env python3
"""Run the GDB-GL-0 loop against an already-running pilot server."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "clients" / "python"))
from graphenedb_client import GrapheneDBClient, GrapheneDBError  # noqa: E402


def policy(version: str, candidates: int, confidence: float) -> dict:
    return {
        "version": version,
        "semantic_candidates": candidates,
        "max_hops": 6,
        "max_paths": 32,
        "max_paths_per_root": 8,
        "max_visited_states": 20000,
        "max_opposition_rounds": 1,
        "minimum_confidence": confidence,
        "reexpansion_threshold": 0.25,
    }


def episode(
    episode_id: str,
    split: str,
    retrieval_policy: dict,
    success: bool,
    quality: float,
    evidence_nodes: list[int],
) -> dict:
    return {
        "schema_version": 1,
        "tenant_id": "docker-merchant-risk",
        "episode_id": episode_id,
        "family": "incident-diagnosis",
        "domain": "checkout",
        "split": split,
        "query": "Why did checkout latency increase?",
        "signature": 33,
        "model_version": "reasoner-1",
        "policy": retrieval_policy,
        "outcome_verified": True,
        "verifier_id": "postmortem-reviewer",
        "outcome_evidence_id": "postmortem-docker-42",
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


def decision(event_id: str, action: str, retrieval_policy: dict) -> dict:
    return {
        "schema_version": 1,
        "tenant_id": "docker-merchant-risk",
        "event_id": event_id,
        "action": action,
        "policy": retrieval_policy,
        "approved": True,
        "approver_id": "memory-governance-board",
        "evaluation_reference": "eval://GDB-GL-0/production-docker",
        "reason": "passed governed development and safety gates",
    }


parser = argparse.ArgumentParser()
parser.add_argument("--base-url", required=True)
parser.add_argument("--api-key", required=True)
parser.add_argument("--verify-restart", action="store_true")
arguments = parser.parse_args()
client = GrapheneDBClient(arguments.base_url, api_key=arguments.api_key)

if arguments.verify_restart:
    current = client.current_learning_policy("docker-merchant-risk")
    assert current.data["policy"]["version"] == "baseline-v1"
    print(
        json.dumps(
            {
                "production_docker_restart_verified": True,
                "current_policy": "baseline-v1",
                "server_user_contract": "checked externally with docker inspect",
            },
            sort_keys=True,
        )
    )
    raise SystemExit(0)

extraction = client.put_extraction(
    {
        "schema_version": 1,
        "source_id": "production-docker-learning-evidence",
        "source_uri": "https://evidence.example/postmortem-docker-42",
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
                "evidence_id": "postmortem-docker-42",
                "evidence_text": "pool wait preceded latency",
            }
        ],
    }
)
evidence_nodes = list(extraction.data["external_to_node_id"].values())
baseline = policy("baseline-v1", 8, 0.55)
candidate = policy("candidate-v2", 16, 0.45)
episode_specs = [
    ("baseline-train-1", "training", baseline, False, 0.45),
    ("baseline-train-2", "training", baseline, True, 0.60),
    ("baseline-dev-1", "development", baseline, False, 0.45),
    ("candidate-train-1", "training", candidate, True, 0.92),
    ("candidate-train-2", "training", candidate, True, 0.95),
    ("candidate-dev-1", "development", candidate, True, 0.96),
    ("candidate-eval-1", "evaluation", candidate, True, 1.0),
]
for values in episode_specs:
    client.record_learning_episode(episode(*values, evidence_nodes))

evaluation = client.evaluate_learning_policies(
    "docker-merchant-risk", baseline
)
assert evaluation.data["has_recommendation"] is True
assert evaluation.data["recommended"]["version"] == "candidate-v2"
assert evaluation.data["evaluation_episodes_excluded"] == 1
assert evaluation.data["durable_writes"] is False

unapproved = decision("unapproved", "promote", candidate)
unapproved["approved"] = False
try:
    client.decide_learning_policy(unapproved)
    raise AssertionError("unapproved policy decision was accepted")
except GrapheneDBError as error:
    assert error.status == 400

client.decide_learning_policy(
    decision("promote-baseline", "promote", baseline)
)
client.decide_learning_policy(
    decision("promote-candidate", "promote", candidate)
)
hypotheses = client.reason_hypokosh(
    "checkout latency increased",
    tenant_id="docker-merchant-risk",
    use_active_policy=True,
    signature=33,
)
assert hypotheses.data["active_policy"]["version"] == "candidate-v2"
assert hypotheses.data["durable_writes"] is False
assert all(
    proposal["origin"] == "hypothetical"
    and proposal["eligible_for_truth_promotion"] is False
    for proposal in hypotheses.data["proposals"]
)
client.decide_learning_policy(
    decision("rollback-baseline", "rollback", baseline)
)

print(
    json.dumps(
        {
            "production_docker_learning_loop_passed": True,
            "episodes": len(episode_specs),
            "evaluation_episodes_excluded": 1,
            "recommended_policy": "candidate-v2",
            "hypotheses": len(hypotheses.data["proposals"]),
            "rolled_back_to": "baseline-v1",
        },
        sort_keys=True,
    )
)
