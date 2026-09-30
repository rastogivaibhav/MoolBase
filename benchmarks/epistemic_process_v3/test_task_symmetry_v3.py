#!/usr/bin/env python3
from __future__ import annotations
import json
from collections import defaultdict
from pathlib import Path

ROOT=Path(__file__).resolve().parent
doc=json.loads((ROOT/"tasks_v3_candidate.json").read_text())

def swap_h(x):
    if x=="H1": return "H2"
    if x=="H2": return "H1"
    return x

def event_signature(e):
    return {
        "kind":e["kind"],"bears_on":e["bears_on"],
        "independent":e["independent"],"semantic_verification":e["semantic_verification"],
        "material":e["material"],"depends_count":len(e.get("depends_on",[])),
        "revokes_count":len(e.get("revokes",[])),"supersedes_count":len(e.get("supersedes",[])),
    }

groups=defaultdict(list)
for ep in doc["episodes"]: groups[ep["mirror_group"]].append(ep)
assert len(groups)==24*8
for key,pair in groups.items():
    assert len(pair)==2,key
    a,b=sorted(pair,key=lambda x:x["variant"])
    assert a["mirror_primary"]=="H1" and b["mirror_primary"]=="H2",key
    ae=a["runtime"]["events"]+a["environment"]["latent_events"]
    be=b["runtime"]["events"]+b["environment"]["latent_events"]
    assert len(ae)==len(be),key
    for x,y in zip(ae,be):
        sx,sy=event_signature(x),event_signature(y)
        assert swap_h(sx.pop("bears_on"))==sy.pop("bears_on"),key
        assert sx==sy,key
    ao,bo=a["oracle"],b["oracle"]
    assert swap_h(ao["expected_terminal_operative"])==bo["expected_terminal_operative"],key
    assert swap_h(ao["expected_terminal_committed"])==bo["expected_terminal_committed"],key
    assert ao["expected_status"]==bo["expected_status"],key
    assert ao["expected_abstention"]==bo["expected_abstention"],key
    if ao["expected_revision"]:
        assert bo["expected_revision"]
        assert swap_h(ao["expected_revision"]["from"])==bo["expected_revision"]["from"],key
        assert swap_h(ao["expected_revision"]["to"])==bo["expected_revision"]["to"],key

byfam=defaultdict(list)
for ep in doc["episodes"]: byfam[ep["task_family"]].append(ep)
for fam in ("node_id_permutation_tie","target_insertion_permutation"):
    for a,b in zip(byfam[fam][0::2],byfam[fam][1::2]):
        assert a["runtime"]["target_layout"]["target_insertion_order"] == ["H1","H2"]
        assert b["runtime"]["target_layout"]["target_insertion_order"] == ["H2","H1"]
for a,b in zip(byfam["evidence_order_permutation"][0::2],byfam["evidence_order_permutation"][1::2]):
    assert a["runtime"]["events"][0]["bears_on"]=="H1"
    assert b["runtime"]["events"][0]["bears_on"]=="H2"
print("cycle3_symmetry=passed mirror_pairs=192")
