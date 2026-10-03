#!/usr/bin/env python3
"""Fail-closed mechanical validation for V3 Cycle-5 blind campaign."""
from __future__ import annotations
import argparse, collections, json
from pathlib import Path
from typing import Any

REQUIRED_ROUND={
 "round_index","trigger","dialectical_challenge_present",
 "corroboration_search_requested","expansion_opportunity_available",
 "expansion_opportunity_class","options_before","options_after",
 "bundle_hash_before","bundle_hash_after","visited_states_before",
 "visited_states_after","frontier_changed","has_answer_before",
 "has_answer_after","operative_hypothesis_before","operative_hypothesis_after",
 "committed_answer_before","committed_answer_after","status_before","status_after",
 "target_ranking_before","target_ranking_after","rank_changed",
 "operative_hypothesis_changed","status_changed","committed_answer_changed",
 "stop_reason",
}
FORBIDDEN={"oracle","correctness","accuracy","claim_status","task_family","expected_terminal_operative"}

def scan(value:Any,path="root"):
    if isinstance(value,dict):
        for k,v in value.items():
            if k in FORBIDDEN or k.startswith("expected_"):
                raise AssertionError(f"oracle/outcome key {path}.{k}")
            scan(v,f"{path}.{k}")
    elif isinstance(value,list):
        for i,v in enumerate(value): scan(v,f"{path}[{i}]")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--raw",required=True)
    p.add_argument("--blind-input",required=True)
    args=p.parse_args()
    raw=json.loads(Path(args.raw).read_text())
    blind=json.loads(Path(args.blind_input).read_text())
    scan(raw)
    assert raw["score_bearing"] is False
    assert raw["score_bearing_authorized"] is False
    assert raw["oracle_join_performed"] is False
    expected_ids=[e["id"] for e in blind["episodes"]]
    assert len(expected_ids)==384
    counts=collections.Counter(e["configuration"] for e in raw["episodes"])
    assert counts=={"G0E":384,"G1":384,"G2":384},counts
    by_cfg=collections.defaultdict(set)
    failures=gaps=rounds=0
    event_counts=collections.Counter()
    opportunity_counts=collections.Counter()
    latent_discovery=0
    latent_ids={e["id"]:{x["id"] for x in e["harness_environment"]["latent_events"]}
                for e in blind["episodes"]}
    visible_step_counts={e["id"]:len(e["runtime"]["events"]) for e in blind["episodes"]}
    for ep in raw["episodes"]:
        cfg=ep["configuration"]; eid=ep["episode_id"]
        by_cfg[cfg].add(eid)
        failures+=bool(ep.get("failures"))
        assert len(ep.get("steps",[]))==visible_step_counts[eid],(cfg,eid)
        discovered=False
        for step in ep.get("steps",[]):
            gaps+=len(step.get("telemetry_gaps",[]))
            refs=set(step.get("active_evidence_refs",[]))
            if refs & latent_ids[eid]: discovered=True
            for ev in step.get("native_events",[]):
                event_counts[(cfg,str(ev.get("type")))] += 1
            for rr in step.get("recovery_rounds",[]):
                rounds+=1
                missing=REQUIRED_ROUND-set(rr)
                assert not missing,(cfg,eid,sorted(missing))
                opportunity_counts[(cfg,rr["expansion_opportunity_class"])]+=1
                for key in ("dialectical_challenge_present",
                            "corroboration_search_requested",
                            "expansion_opportunity_available",
                            "frontier_changed","rank_changed",
                            "operative_hypothesis_changed","status_changed",
                            "committed_answer_changed"):
                    assert isinstance(rr[key],bool),(cfg,eid,key)
        if cfg=="G2" and discovered: latent_discovery+=1
    assert failures==0,failures
    assert gaps==0,gaps
    for cfg in ("G0E","G1","G2"):
        assert by_cfg[cfg]==set(expected_ids),cfg
    assert rounds>0
    # Presence of ordinary recovery fields is not native transition coverage.
    for cfg in ("G1", "G2"):
        for event in ("revision", "decommitment", "recommitment", "resolution"):
            assert event_counts[(cfg,event)] > 0, (cfg,event,"missing native transition coverage")
    for event in ("revision", "decommitment", "recommitment", "resolution", "challenge", "reopen"):
        assert event_counts[("G0E",event)] == 0, ("G0E",event)
    assert event_counts[("G2","challenge")]>0,event_counts
    assert event_counts[("G2","reopen")]>0,event_counts
    assert event_counts[("G2","corroboration_search")]>0,event_counts
    assert event_counts[("G1","challenge")]==0,event_counts
    assert event_counts[("G1","reopen")]==0,event_counts
    assert opportunity_counts[("G2","NEW_ELIGIBLE_NODE_AVAILABLE")]>0,opportunity_counts
    assert opportunity_counts[("G2","NO_EXPANSION_OPPORTUNITY")]>0,opportunity_counts
    assert latent_discovery>0,latent_discovery
    print(json.dumps({
      "cycle5_blind_validation":"passed",
      "executions":len(raw["episodes"]),
      "runtime_failures":failures,"telemetry_gaps":gaps,
      "recovery_rounds":rounds,
      "g2_challenges":event_counts[("G2","challenge")],
      "g2_reopens":event_counts[("G2","reopen")],
      "g2_corroboration_searches":event_counts[("G2","corroboration_search")],
      "g2_latent_discovery_episodes":latent_discovery,
      "score_bearing":False,"oracle_join_performed":False,
    },sort_keys=True))

if __name__=="__main__":
    main()
