#!/usr/bin/env python3
from __future__ import annotations
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
doc=json.loads((ROOT/"tasks_v3_candidate.json").read_text())
positive={"dwm_latent_evidence_positive","dwm_insufficiency_no_opposition","dwm_genuine_opposition"}
pos=neg=0
for ep in doc["episodes"]:
    fam=ep["task_family"]; topo=ep["environment"]["search_topology"]
    visible={e["id"] for e in ep["runtime"]["events"]}
    latent={e["id"] for e in ep["environment"]["latent_events"]}
    if fam in positive:
        pos+=1
        assert latent and visible.isdisjoint(latent)
        assert set(topo["initial_frontier"])==visible
        assert set(topo["latent_frontier"])==latent
        assert topo["frontier_exhausted_initially"] is False
        assert topo["expansion_opportunities"]
        pre=set(topo["initial_frontier"])
        assert not (pre & latent)
        post=set(pre)
        for edge in topo["edges"]:
            assert edge["from"] in pre
            assert edge["to"] in latent
            assert edge["admissible"] and not edge["initially_reachable"] and edge["requires_expansion"]
            post.add(edge["to"])
        assert latent <= post
        assert ep["oracle"]["expected_dwm_useful_reopen"] is True
    if fam=="dwm_exhausted_frontier_negative":
        neg+=1
        assert not latent
        assert topo["frontier_exhausted_initially"] is True
        assert topo["latent_frontier"]==[] and topo["edges"]==[]
        assert topo["expansion_opportunities"]==[]
        assert ep["oracle"]["expected_dwm_useful_reopen"] is False
assert pos==48 and neg==16,(pos,neg)
print("cycle3_dwm_topology=passed positive=48 exhausted_negative=16")
