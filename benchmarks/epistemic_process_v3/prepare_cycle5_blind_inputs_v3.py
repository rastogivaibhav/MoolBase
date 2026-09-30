#!/usr/bin/env python3
"""Build the oracle-free V3 Cycle-5 execution package.

The harness may retain structural latent-frontier information required to stage
the production graph, but it deliberately drops family labels, claims, mirrors,
scientific-purpose text, and the evaluator oracle.
"""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from typing import Any

FORBIDDEN_KEYS={
    "oracle","task_family","mirror_primary","mirror_group","scientific_purpose",
    "claim_ids","expected_terminal_operative","expected_terminal_committed",
    "expected_status","expected_abstention","challenge_warranted_steps",
    "corroboration_search_warranted_steps","revision_required",
    "expected_revision","resolution_eligible","expected_dwm_useful_reopen",
    "expected_expansion_opportunity_class","latent_evidence_ids","correctness",
}

def scan(value: Any, path="root"):
    if isinstance(value,dict):
        for k,v in value.items():
            if k in FORBIDDEN_KEYS or k.startswith("expected_"):
                raise AssertionError(f"oracle/evaluator key leaked at {path}.{k}")
            scan(v,f"{path}.{k}")
    elif isinstance(value,list):
        for i,v in enumerate(value): scan(v,f"{path}[{i}]")

def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value,separators=(",",":"),sort_keys=True)+"\n").encode()

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--tasks",required=True)
    p.add_argument("--out",required=True)
    args=p.parse_args()
    source=json.loads(Path(args.tasks).read_text())
    episodes=[]
    for ep in source["episodes"]:
        layout=ep["runtime"]["target_layout"]
        runtime={
            "target_layout":{
                "target_insertion_order":list(layout["target_insertion_order"]),
                "padding_nodes_before_targets":int(layout["padding_nodes_before_targets"]),
            },
            "events":ep["runtime"]["events"],
            "initial_search_limits":ep["runtime"]["initial_search_limits"],
        }
        environment={
            "latent_events":ep["environment"]["latent_events"],
            "search_topology":ep["environment"]["search_topology"],
        }
        episodes.append({
            "id":ep["id"],
            "variant":ep["variant"],
            "runtime":runtime,
            "harness_environment":environment,
        })
    out={
        "schema":"epistemic-process-v3-cycle5-blind-input-v1",
        "mode":"blind-unscored-mechanical",
        "experiment_id_candidate":source["experiment_id_candidate"],
        "generator_seed":source["generator_seed"],
        "score_bearing":False,
        "score_bearing_authorized":False,
        "oracle_join_performed":False,
        "episode_count":len(episodes),
        "episodes":episodes,
    }
    scan(out)
    payload=canonical_bytes(out)
    Path(args.out).write_bytes(payload)
    print(json.dumps({
        "episodes":len(episodes),
        "oracle_fields_present":False,
        "score_bearing":False,
        "sha256":hashlib.sha256(payload).hexdigest(),
    },sort_keys=True))

if __name__=="__main__":
    main()
