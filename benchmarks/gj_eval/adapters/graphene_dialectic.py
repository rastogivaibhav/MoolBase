#!/usr/bin/env python3
"""G1 Graphene causal/dialectic traversal with reopening disabled."""

from __future__ import annotations

import json
import sys

import graphene_http as g


def main() -> int:
    task = json.load(sys.stdin)
    extraction, root_external = g.build_extraction(task)
    imported = g.request("/v1/extractions", extraction)
    mapping = imported.get("external_to_node_id", {})
    root_by_node = {
        int(mapping[external]): choice
        for choice, external in root_external.items()
        if external in mapping
    }
    result = g.request("/v1/reason/dialectic", {
        "query": "Checkout failures increased.",
        "signature": g.signature_for(task["world_id"]),
        "mode": "balanced",
        "semantic_candidates": 16,
        "max_hops": 5,
        "max_paths": 128,
        "max_paths_per_root": 32,
        "max_visited_states": 50000,
        "max_opposition_rounds": 0,
        "minimum_confidence": 0.30,
        "reexpansion_threshold": 0.20,
    })
    synthesis = result.get("synthesis", {})
    choice = root_by_node.get(int(synthesis.get("primary_node", -1)), "unknown")
    status = str(synthesis.get("epistemic_status", "abstain"))
    act = "act" if status == "supported" and choice != "unknown" else (
        "abstain" if status == "abstain" else "review"
    )
    print(json.dumps({
        "root_choice": choice,
        "act": act,
        "selected_confidence": float(synthesis.get("confidence", 0.0)),
        "epistemic_status": status,
        "receipt": {
            "dialectic": result,
            "max_opposition_rounds": 0,
            "extraction": {
                "inserted_nodes": len(imported.get("inserted_node_ids", [])),
                "existing_nodes": len(imported.get("existing_node_ids", [])),
                "inserted_edges": len(imported.get("inserted_edge_ids", [])),
            },
        },
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
