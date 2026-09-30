#!/usr/bin/env python3
"""Produce reasoner-safe EP-PROCESS-V3 input without evaluator oracle/future evidence."""
from __future__ import annotations
import json, hashlib
from pathlib import Path

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT/"tasks_v3_candidate.json"

def strip_document(doc):
    episodes=[]
    for ep in doc["episodes"]:
        layout=ep["runtime"]["target_layout"]
        episodes.append({
            "id":ep["id"],
            "task_family":ep["task_family"],
            "variant":ep["variant"],
            "runtime":{
                "target_layout":{
                    "target_insertion_order":layout["target_insertion_order"],
                    "padding_nodes_before_targets":layout["padding_nodes_before_targets"],
                },
                "events":ep["runtime"]["events"],
                "initial_search_limits":ep["runtime"]["initial_search_limits"],
            },
        })
    return {
        "schema":"epistemic-process-v3-runtime-task-view-v1",
        "status":"UNSCORED_RUNTIME_SAFE_VIEW",
        "experiment_id_candidate":doc["experiment_id_candidate"],
        "generator_seed":doc["generator_seed"],
        "score_bearing_authorized":False,
        "episode_count":len(episodes),
        "episodes":episodes,
    }

def canonical_bytes(doc):
    return (json.dumps(doc,separators=(",",":"),sort_keys=True)+"\n").encode()

def main():
    doc=json.loads(SOURCE.read_text())
    out=strip_document(doc)
    payload=canonical_bytes(out)
    target=ROOT/"tasks_v3_runtime_view.json"
    target.write_bytes(payload)
    print(f"runtime_view={target} episodes={len(out['episodes'])} sha256={hashlib.sha256(payload).hexdigest()}")

if __name__=="__main__":
    main()
