#!/usr/bin/env python3
"""Build Cycle-5 blind inputs from the frozen V3 task candidate.

Produces:
1. reasoner-safe runtime tasks via the frozen Cycle-3 stripper;
2. harness-only latent graph fixtures containing no oracle/outcome labels.

The harness environment may configure graph topology but is never passed as
reasoner evidence or as an evaluator truth source.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from strip_runtime_tasks_v3 import strip_document, canonical_bytes

FORBIDDEN = {
    "oracle","task_family","claim_ids","scientific_purpose","mirror_primary",
    "mirror_group","expected_terminal_operative","expected_terminal_committed",
    "expected_status","expected_abstention","challenge_warranted_steps",
    "corroboration_search_warranted_steps","revision_required",
    "expected_revision","resolution_eligible","expected_dwm_useful_reopen",
    "expected_expansion_opportunity_class","latent_evidence_ids",
}

def scan(value, path="root"):
    if isinstance(value, dict):
        for key, child in value.items():
            if key in FORBIDDEN:
                raise AssertionError(f"forbidden blind field {path}.{key}")
            scan(child, f"{path}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            scan(child, f"{path}[{index}]")

def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--runtime-out", required=True)
    parser.add_argument("--environment-out", required=True)
    args=parser.parse_args()

    source=json.loads(Path(args.tasks).read_text(encoding="utf-8"))
    runtime=strip_document(source)
    scan(runtime)

    environment={
        "schema":"epistemic-process-v3-cycle5-harness-environment-v1",
        "status":"UNSCORED_HARNESS_ONLY",
        "experiment_id_candidate":source["experiment_id_candidate"],
        "generator_seed":source["generator_seed"],
        "score_bearing_authorized":False,
        "oracle_join_permitted":False,
        "episodes":[
            {
                "id":ep["id"],
                "latent_events":ep["environment"]["latent_events"],
            }
            for ep in source["episodes"]
        ],
    }
    scan(environment)

    runtime_path=Path(args.runtime_out)
    runtime_path.parent.mkdir(parents=True,exist_ok=True)
    runtime_bytes=canonical_bytes(runtime)
    runtime_path.write_bytes(runtime_bytes)

    env_path=Path(args.environment_out)
    env_path.parent.mkdir(parents=True,exist_ok=True)
    env_bytes=(json.dumps(environment,separators=(",",":"),sort_keys=True)+"\n").encode()
    env_path.write_bytes(env_bytes)

    assert len(runtime["episodes"]) == 384
    assert len(environment["episodes"]) == 384
    assert [e["id"] for e in runtime["episodes"]] == [e["id"] for e in environment["episodes"]]
    print(json.dumps({
        "episodes":384,
        "runtime_sha256":hashlib.sha256(runtime_bytes).hexdigest(),
        "environment_sha256":hashlib.sha256(env_bytes).hexdigest(),
        "oracle_fields_present":False,
        "score_bearing_authorized":False,
    },sort_keys=True))

if __name__=="__main__":
    main()
