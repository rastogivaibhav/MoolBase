#!/usr/bin/env python3
"""Validate V3 Cycle-5 blind mechanical evidence without evaluating correctness."""
from __future__ import annotations
import argparse, collections, json
from pathlib import Path

CONFIGS={"C0","G0E","C1","G1","G2"}
REQUIRED_STEP={
    "step","ingested_evidence_ids","final_state","active_evidence_refs",
    "native_events","recovery_rounds","telemetry_gaps","execution",
}
REQUIRED_STATE={
    "has_answer","operative_hypothesis","committed_answer","epistemic_status",
    "bundle_hash","visited_states","truncated","evidence_family_ids",
}
REQUIRED_ROUND={
    "round_index","dialectical_challenge_present",
    "corroboration_search_requested","recovery_search_requested",
    "opposition_search_requested","expansion_opportunity_available",
    "expansion_opportunity_class","bundle_hash_before","bundle_hash_after",
    "visited_states_before","visited_states_after","frontier_changed",
    "has_answer_before","has_answer_after","operative_hypothesis_before",
    "operative_hypothesis_after","committed_answer_before",
    "committed_answer_after","status_before","status_after",
    "operative_hypothesis_changed","status_changed",
    "committed_answer_changed","stop_reason",
}

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--raw",required=True)
    p.add_argument("--runtime-tasks",required=True)
    p.add_argument("--environment",required=True)
    a=p.parse_args()
    raw=load(a.raw); tasks=load(a.runtime_tasks); env=load(a.environment)

    assert raw["score_bearing"] is False
    assert raw["score_bearing_authorized"] is False
    assert raw["oracle_join_performed"] is False
    assert raw["outcome_metrics_computed"] is False
    assert tasks["score_bearing_authorized"] is False
    assert env["score_bearing_authorized"] is False
    assert env["oracle_join_permitted"] is False
    assert raw["episode_count"]==384
    assert raw["configuration_episode_total"]==1920

    task_by={e["id"]:e for e in tasks["episodes"]}
    env_by={e["id"]:e for e in env["episodes"]}
    assert set(task_by)==set(env_by)
    assert len(task_by)==384

    by_config=collections.Counter()
    seen_pairs=set()
    failures=0; gaps=0; rounds=0
    event_counts=collections.Counter()
    opportunity_counts=collections.Counter()
    frontier_changes=0
    latent_g2_episodes=0
    latent_g2_discovered=0

    for ep in raw["episodes"]:
        eid=ep["episode_id"]; cfg=ep["configuration"]
        assert cfg in CONFIGS
        assert eid in task_by
        pair=(eid,cfg)
        assert pair not in seen_pairs, f"duplicate pair {pair}"
        seen_pairs.add(pair); by_config[cfg]+=1
        if ep.get("failures"): failures+=1
        expected_steps=len(task_by[eid]["runtime"]["events"])
        assert len(ep.get("steps") or [])==expected_steps, (eid,cfg)
        latent_ids={x["id"] for x in env_by[eid].get("latent_events",[])}
        discovered=set()

        for step in ep.get("steps") or []:
            missing=REQUIRED_STEP-set(step)
            assert not missing,(eid,cfg,step.get("step"),sorted(missing))
            assert not step["telemetry_gaps"],(eid,cfg,step["step"])
            gaps += len(step["telemetry_gaps"])
            state=step["final_state"]
            missing_state=REQUIRED_STATE-set(state)
            assert not missing_state,(eid,cfg,step["step"],sorted(missing_state))
            if cfg in {"C0","C1","G0E"}:
                assert state["committed_answer"] is None,(eid,cfg,step["step"])
            if cfg in {"C0","C1"}:
                assert step.get("common_head",{}).get("decision_head")=="epistemic-process-v2-common-head-v1"
            discovered.update(set(step.get("active_evidence_refs",[])) & latent_ids)
            for ev in step.get("native_events",[]):
                typ=str(ev.get("type") or "")
                assert typ, (eid,cfg,step["step"])
                event_counts[(cfg,typ)] += 1
            for rr in step.get("recovery_rounds",[]):
                rounds+=1
                missing_rr=REQUIRED_ROUND-set(rr)
                assert not missing_rr,(eid,cfg,step["step"],sorted(missing_rr))
                opportunity_counts[(cfg,str(rr["expansion_opportunity_class"]))]+=1
                if rr["frontier_changed"]: frontier_changes+=1
                for key in (
                    "dialectical_challenge_present","corroboration_search_requested",
                    "recovery_search_requested","opposition_search_requested",
                    "expansion_opportunity_available","frontier_changed",
                    "has_answer_before","has_answer_after",
                    "operative_hypothesis_changed","status_changed",
                    "committed_answer_changed",
                ):
                    assert isinstance(rr[key],bool),(eid,cfg,step["step"],key)

        if cfg=="G2" and latent_ids:
            latent_g2_episodes+=1
            if discovered: latent_g2_discovered+=1

    assert failures==0, failures
    assert gaps==0, gaps
    assert len(seen_pairs)==1920
    assert by_config==collections.Counter({c:384 for c in CONFIGS}),by_config
    assert rounds>0
    assert event_counts[("G2","challenge")]>0, event_counts
    assert (
        event_counts[("G1","corroboration_search")] +
        event_counts[("G2","corroboration_search")]
    )>0, event_counts
    assert event_counts[("G2","reopen")]>0, event_counts
    assert sum(v for (cfg,cls),v in opportunity_counts.items()
               if cfg=="G2" and cls!="NO_EXPANSION_OPPORTUNITY")>0
    assert frontier_changes>0
    assert latent_g2_episodes==48, latent_g2_episodes
    assert latent_g2_discovered>0, "no G2 latent evidence discovered"

    print(json.dumps({
        "cycle5_blind_mechanical_validation":"passed",
        "configuration_episode_counts":dict(sorted(by_config.items())),
        "runtime_failures":failures,
        "telemetry_gaps":gaps,
        "recovery_rounds":rounds,
        "g2_challenges":event_counts[("G2","challenge")],
        "corroboration_search_events":event_counts[("G1","corroboration_search")]+event_counts[("G2","corroboration_search")],
        "g2_reopens":event_counts[("G2","reopen")],
        "frontier_changes":frontier_changes,
        "latent_g2_episodes":latent_g2_episodes,
        "latent_g2_discovered":latent_g2_discovered,
        "score_bearing":False,
        "oracle_join_performed":False,
    },sort_keys=True))

if __name__=="__main__":
    main()
