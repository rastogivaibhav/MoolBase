#!/usr/bin/env python3
"""GJ-Eval adapter for the current GrapheneDB HTTP pilot/runtime.

Reads one structured GJ-Eval task on stdin and writes one adapter decision on
stdout. A GrapheneDB server must already be running.

Environment:
  GRAPHENEDB_URL      default http://127.0.0.1:8080
  GRAPHENEDB_API_KEY  optional
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
import urllib.error
import urllib.request
from typing import Any


BASE_URL = os.environ.get("GRAPHENEDB_URL", "http://127.0.0.1:8080").rstrip("/")
API_KEY = os.environ.get("GRAPHENEDB_API_KEY", "")


def request(path: str, payload: dict[str, Any]) -> dict[str, Any]:
    data = json.dumps(payload, sort_keys=True).encode("utf-8")
    headers = {"Accept": "application/json", "Content-Type": "application/json"}
    if API_KEY:
        headers["X-API-Key"] = API_KEY
    req = urllib.request.Request(
        BASE_URL + path, data=data, headers=headers, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            raw = response.read().decode("utf-8")
            body = json.loads(raw) if raw else {}
            if response.status < 200 or response.status >= 300:
                raise RuntimeError(f"{path} returned HTTP {response.status}: {body}")
            return body
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"{path} returned HTTP {exc.code}: {raw}") from exc


def signature_for(world_id: str) -> int:
    value = int.from_bytes(hashlib.sha256(world_id.encode()).digest()[:8], "big")
    return value & ((1 << 63) - 1) or 1


def incident_for(world_id: str) -> int:
    return int.from_bytes(hashlib.sha256(("incident:" + world_id).encode()).digest()[:4], "big")


def observed_at(value: Any, timestep: int) -> str:
    if isinstance(value, str) and value:
        return value
    return f"2026-01-01T00:00:{timestep:02d}Z"


def evidence_metadata(obs: dict[str, Any], timestep: int) -> dict[str, str]:
    metadata = {
        "source_id": str(obs.get("source_id") or obs.get("evidence_id") or "unknown-source"),
        "evidence_family_id": str(
            obs.get("source_family") or obs.get("source_id") or obs.get("evidence_id") or "unknown-family"
        ),
        "observed_at": observed_at(obs.get("observed_at"), timestep),
        "evidence_content_hash": hashlib.sha256(
            str(obs.get("claim", "")).encode("utf-8")
        ).hexdigest(),
    }
    derived = obs.get("derived_from")
    if derived:
        metadata["derived_from"] = str(derived)
        metadata["derivation_id"] = str(derived)
    return metadata


def build_extraction(task: dict[str, Any]) -> tuple[dict[str, Any], dict[str, str]]:
    world_id = task["world_id"]
    timestep = int(task["timestep"])
    visible = task.get("visible_state")
    if not isinstance(visible, list) or (
        visible and not isinstance(visible[0], dict)
    ):
        raise ValueError("Graphene adapter requires --state-mode structured")

    signature = signature_for(world_id)
    incident = incident_for(world_id)
    source_id = f"gj-eval:{world_id}"

    root_external: dict[str, str] = {}
    nodes: list[dict[str, Any]] = []
    for choice in task["choices"]:
        choice_id = str(choice["id"])
        if choice_id == "unknown":
            continue
        external = f"root/{choice_id}"
        root_external[choice_id] = external
        nodes.append(
            {
                "external_id": external,
                "content": f"{choice['label']} is the primary cause of checkout failures.",
                "signature": signature,
                "incident": incident,
                "role": "root",
                "metadata": {"gj_choice_id": choice_id, "gj_world_id": world_id},
            }
        )

    symptom_external = "symptom/checkout-failures"
    symptom_claim = "Checkout failures increased."
    for obs in visible:
        if not obs.get("supports") and not obs.get("contradicts"):
            symptom_claim = str(obs.get("claim") or symptom_claim)
            break
    nodes.append(
        {
            "external_id": symptom_external,
            "content": symptom_claim,
            "signature": signature,
            "incident": incident,
            "role": "symptom",
            "metadata": {"gj_role": "symptom", "gj_world_id": world_id},
        }
    )

    relations: list[dict[str, Any]] = []
    known_evidence: set[str] = set()
    for obs in visible:
        eid = str(obs.get("evidence_id") or "")
        if not eid:
            continue
        supports = obs.get("supports")
        contradicts = obs.get("contradicts")
        if not supports and not contradicts and not obs.get("supersedes") and not obs.get("invalidates"):
            continue

        evidence_external = f"evidence/{eid}"
        known_evidence.add(eid)
        md = evidence_metadata(obs, timestep)
        nodes.append(
            {
                "external_id": evidence_external,
                "content": str(obs.get("claim") or eid),
                "signature": signature,
                "incident": incident,
                "role": "node",
                "metadata": {
                    "gj_evidence_id": eid,
                    "gj_source_id": md["source_id"],
                    "gj_source_family": md["evidence_family_id"],
                },
            }
        )

        evidence_id = eid
        confidence = float(obs.get("confidence", 0.8))
        origin = str(obs.get("origin") or "observed")

        # Every evidence node that participates in a hypothesis path reaches the
        # symptom. Root -> evidence -> symptom preserves multiple independent
        # routes instead of collapsing all support into one root/symptom edge.
        if supports in root_external:
            relations.append(
                {
                    "from_external_id": root_external[str(supports)],
                    "to_external_id": evidence_external,
                    "origin": origin,
                    "role": "supports",
                    "confidence": confidence,
                    "evidence_id": evidence_id,
                    "evidence_text": str(obs.get("claim") or ""),
                    "metadata": md,
                }
            )
        if contradicts in root_external:
            relations.append(
                {
                    "from_external_id": root_external[str(contradicts)],
                    "to_external_id": evidence_external,
                    "origin": origin,
                    "role": "contradicts",
                    "confidence": confidence,
                    "evidence_id": evidence_id,
                    "evidence_text": str(obs.get("claim") or ""),
                    "metadata": md,
                }
            )
        if supports in root_external or contradicts in root_external:
            relations.append(
                {
                    "from_external_id": evidence_external,
                    "to_external_id": symptom_external,
                    "origin": origin,
                    "role": "supports",
                    "confidence": confidence,
                    "evidence_id": evidence_id,
                    "evidence_text": str(obs.get("claim") or ""),
                    "metadata": md,
                }
            )

    # Preserve explicit revision structure when the referenced evidence is
    # already visible. These relations are additional world-state structure;
    # they do not delete earlier evidence.
    for obs in visible:
        eid = str(obs.get("evidence_id") or "")
        if not eid or eid not in known_evidence:
            continue
        current = f"evidence/{eid}"
        for field, role in (("supersedes", "supersedes"), ("invalidates", "contradicts")):
            prior = obs.get(field)
            if prior and str(prior) in known_evidence:
                md = evidence_metadata(obs, timestep)
                relations.append(
                    {
                        "from_external_id": current,
                        "to_external_id": f"evidence/{prior}",
                        "origin": str(obs.get("origin") or "observed"),
                        "role": role,
                        "confidence": float(obs.get("confidence", 1.0)),
                        "evidence_id": eid,
                        "evidence_text": str(obs.get("claim") or ""),
                        "metadata": md,
                    }
                )

    extraction = {
        "schema_version": 1,
        "source_id": source_id,
        "extraction_run_id": f"{world_id}:t{timestep}",
        "incident": incident,
        "signature": signature,
        "idempotent": True,
        "nodes": nodes,
        "relations": relations,
    }
    return extraction, root_external


def status_to_act(status: str) -> str:
    normalized = status.lower()
    if normalized == "abstain":
        return "abstain"
    if normalized in {"resolved", "supported"}:
        return "act"
    return "review"


def main() -> int:
    task = json.load(sys.stdin)
    extraction, root_external = build_extraction(task)
    imported = request("/v1/extractions", extraction)
    mapping = imported.get("external_to_node_id", {})
    root_by_node = {
        int(mapping[external]): choice
        for choice, external in root_external.items()
        if external in mapping
    }

    runtime_payload = {
        "query": "Checkout failures increased.",
        "signature": signature_for(task["world_id"]),
        "mode": "balanced",
        "semantic_candidates": 16,
        "max_hops": 5,
        "max_paths": 128,
        "max_paths_per_root": 32,
        "max_visited_states": 50000,
        "minimum_confidence": 0.30,
        "reexpansion_threshold": 0.20,
        "max_recursive_cycles": 3,
    }
    runtime = request("/v1/reason/runtime", runtime_payload)
    primary = int(runtime.get("primary_node", -1))
    root_choice = root_by_node.get(primary, "unknown")
    status = str(runtime.get("status", "abstain"))

    output: dict[str, Any] = {
        "root_choice": root_choice,
        "act": status_to_act(status),
        "selected_confidence": float(runtime.get("confidence", 0.0)),
        "epistemic_status": status,
        "receipt": {
            "runtime": runtime,
            "extraction": {
                "inserted_nodes": len(imported.get("inserted_node_ids", [])),
                "existing_nodes": len(imported.get("existing_node_ids", [])),
                "inserted_edges": len(imported.get("inserted_edge_ids", [])),
                "idempotent_replay": bool(imported.get("idempotent_replay", False)),
            },
        },
    }

    # HypoKosh is read-only. Use it to expose actual retained alternatives for
    # Track C rather than synthesizing a ranking from the final answer.
    try:
        hypotheses = request(
            "/v1/reason/hypokosh",
            {
                "query": "Checkout failures increased.",
                "signature": signature_for(task["world_id"]),
                "mode": "balanced",
                "semantic_candidates": 16,
                "max_hops": 5,
                "max_paths": 128,
                "max_paths_per_root": 32,
                "max_visited_states": 50000,
                "max_opposition_rounds": 1,
                "minimum_confidence": 0.30,
                "reexpansion_threshold": 0.20,
                "max_hypotheses": 8,
            },
        )
        ranked: list[str] = []
        for proposal in hypotheses.get("proposals", []):
            choice = root_by_node.get(int(proposal.get("root_node", -1)))
            if choice and choice not in ranked:
                ranked.append(choice)
        if ranked:
            output["ranked_hypotheses"] = ranked
        tests = hypotheses.get("discriminating_tests")
        if tests:
            output["receipt"]["hypokosh_discriminating_tests"] = tests
    except Exception as exc:
        # The primary G6 decision is still valid if the optional Track-C
        # projection fails; retain the failure in the receipt rather than
        # fabricating a hypothesis ranking.
        output["receipt"]["hypokosh_optional_error"] = str(exc)

    print(json.dumps(output, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
