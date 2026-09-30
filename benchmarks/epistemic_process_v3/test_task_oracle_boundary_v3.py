#!/usr/bin/env python3
from __future__ import annotations
import json
from pathlib import Path
from strip_runtime_tasks_v3 import strip_document

ROOT=Path(__file__).resolve().parent
source=json.loads((ROOT/"tasks_v3_candidate.json").read_text())
runtime=strip_document(source)
assert runtime["episode_count"]==384
assert [e["id"] for e in runtime["episodes"]]==[e["id"] for e in source["episodes"]]
forbidden={
 "task_family","oracle","environment","mirror_primary","mirror_group","scientific_purpose",
 "claim_ids","lower_numeric_target","semantic_labels","expected_terminal_operative",
 "expected_terminal_committed","expected_status","expected_abstention","challenge_warranted_steps",
 "corroboration_search_warranted_steps","revision_required","expected_revision",
 "resolution_eligible","expected_dwm_useful_reopen","expected_expansion_opportunity_class",
 "latent_evidence_ids"
}
def scan(v,path="root"):
    if isinstance(v,dict):
        for k,x in v.items():
            assert k not in forbidden,f"forbidden runtime key {path}.{k}"
            scan(x,f"{path}.{k}")
    elif isinstance(v,list):
        for i,x in enumerate(v): scan(x,f"{path}[{i}]")
scan(runtime)
serialized=json.dumps(runtime,sort_keys=True)
for ep in source["episodes"]:
    for latent in ep["environment"]["latent_events"]:
        assert latent["id"] not in serialized, f"latent evidence leaked: {latent['id']}"
print("cycle3_oracle_boundary=passed episodes=384 latent_ids_absent=true")
