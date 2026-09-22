#!/usr/bin/env python3
"""G3 HypoKosh hypothesis-retention arm for GJ-Eval v1.

This arm never claims autonomous action because HypoKosh proposals are explicitly
hypothetical and ineligible for silent truth promotion.
"""

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
    result = g.request("/v1/reason/hypokosh", {
        "query": "Checkout failures increased.",
        "signature": g.signature_for(task["world_id"]),
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
    })
    proposals = sorted(
        result.get("proposals", []),
        key=lambda p: (-float(p.get("plausibility", 0.0)), int(p.get("root_node", 0))),
    )
    ranked = []
    top_choice = "unknown"
    top_plausibility = 0.0
    for proposal in proposals:
        choice = root_by_node.get(int(proposal.get("root_node", -1)))
        if choice and choice not in ranked:
            ranked.append(choice)
            if top_choice == "unknown":
                top_choice = choice
                top_plausibility = float(proposal.get("plausibility", 0.0))

    print(json.dumps({
        "root_choice": top_choice,
        "act": "review",
        "selected_confidence": top_plausibility,
        "ranked_hypotheses": ranked,
        "epistemic_status": "hypothetical",
        "receipt": {
            "hypokosh": result,
            "truth_promotion": False,
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
