#!/usr/bin/env python3
from __future__ import annotations
import copy, json
from pathlib import Path

from evaluate_v3_candidate import CONFIGS, EvaluationInputError, evaluate, validate_inputs

ROOT=Path(__file__).resolve().parent
FULL=json.loads((ROOT/"tasks_v3_candidate.json").read_text())

FAMILIES=[
    "exact_symmetric_tie","near_tie","valid_persistent_history",
    "explicit_revocation","explicit_supersession",
    "refutation_insufficient_replacement","earned_revision",
    "dwm_latent_evidence_positive","dwm_exhausted_frontier_negative",
    "earned_resolution","single_step_control",
]
# exact tie needs a complete mirror pair; every other family uses one variant.
selected=[]
for family in FAMILIES:
    rows=[e for e in FULL["episodes"] if e["task_family"]==family]
    selected.extend(rows[:2] if family=="exact_symmetric_tie" else rows[:1])
TASKS={"episodes":selected}

def active(task,n):
    current={}
    for e in task["runtime"]["events"]:
        if int(e["step"])>n: break
        if e["kind"] in {"support","refute","noop"}: current[e["id"]]=e
        elif e["kind"]=="revoke":
            for ref in e.get("revokes",[]): current.pop(ref,None)
        elif e["kind"]=="supersede":
            for ref in e.get("supersedes",[]): current.pop(ref,None)
            current[e["id"]]=e
    return current

def state(task,cfg,n):
    oracle=task["oracle"]; current=active(task,n)
    terminal=n==len(task["runtime"]["events"])
    if cfg=="G0E":
        op=commit=None; status="evidence_only"; has=False
    elif cfg in {"C0","C1"}:
        op=oracle["expected_terminal_operative"] if terminal else None
        commit=None; status="operative_selected" if op else "open"; has=op is not None
    else:
        op=oracle["expected_terminal_operative"] if terminal else None
        commit=oracle["expected_terminal_committed"] if terminal else None
        status=oracle["expected_status"] if terminal else ("provisional" if op else "open")
        has=op is not None
    return {
        "has_answer":has,"operative_hypothesis":op,"committed_answer":commit,
        "epistemic_status":status,"bundle_hash":n,"visited_states":n,
        "truncated":False,
        "evidence_family_ids":sorted({e["canonical_family_id"] for e in current.values()}),
        "dependency_lineage_ids":sorted({str(x) for e in current.values() for x in e.get("depends_on",[])}),
        "target_ranking":[],
    }

def episode(task,cfg):
    steps=[]
    oracle=task["oracle"]
    for n,event in enumerate(task["runtime"]["events"],start=1):
        final=state(task,cfg,n)
        events=[]
        rounds=[]
        if cfg in {"G1","G2"} and oracle.get("revision_required") and n==oracle["expected_revision"]["replacement_earned_step"]:
            events.append({
                "type":"revision",
                "previous_hypothesis":oracle["expected_revision"]["from"],
                "hypothesis":oracle["expected_revision"]["to"],
            })
        if cfg=="G2" and n in set(oracle.get("challenge_warranted_steps") or []):
            events.append({"type":"challenge","hypothesis":oracle["expected_revision"]["from"] if oracle.get("expected_revision") else None})
            if oracle.get("expected_expansion_opportunity_class"):
                events.append({"type":"reopen"})
                rounds.append({
                    "trigger":"dwm_opposition",
                    "frontier_changed":True,"rank_changed":False,
                    "operative_hypothesis_changed":True,
                    "status_changed":True,"committed_answer_changed":False,
                })
        if cfg=="G2" and n in set(oracle.get("corroboration_search_warranted_steps") or []):
            events.append({"type":"corroboration_search"})
        refs=sorted(active(task,n))
        if cfg=="G2" and terminal and oracle.get("latent_evidence_ids"):
            refs=sorted(set(refs)|set(oracle["latent_evidence_ids"]))
        steps.append({
            "step":n,"ingested_evidence_ids":[event["id"]],
            "final_state":final,"active_evidence_refs":refs,
            "native_events":events,"recovery_rounds":rounds,
            "telemetry_gaps":[],
        })
    return {"episode_id":task["id"],"configuration":cfg,"steps":steps,"failures":[]}

def fixture():
    raw={
        "score_bearing":False,"score_bearing_authorized":False,
        "configurations":list(CONFIGS),
        "episodes":[episode(t,c) for t in selected for c in CONFIGS],
    }
    return copy.deepcopy(TASKS),raw

def rejected(name,mutate):
    tasks,raw=fixture(); mutate(tasks,raw)
    try: validate_inputs(tasks,raw)
    except EvaluationInputError: return
    raise AssertionError(f"accepted adversarial evaluator input: {name}")

def main():
    tasks,raw=fixture()
    result=evaluate(tasks,raw)
    assert result["aggregate"]["G1"]["tie_invariance_rate"]==1.0
    assert result["aggregate"]["G1"]["exact_tie_unique_selection_rate"]==0.0
    assert result["aggregate"]["G0E"]["evidence_retention_exactness"]==1.0
    assert result["aggregate"]["G0E"]["evidence_family_identity_exactness"]==1.0
    assert result["aggregate"]["G0E"]["dependency_lineage_exactness"]==1.0
    assert result["aggregate"]["G0E"]["revocation_visibility_rate"]==1.0
    assert result["aggregate"]["G0E"]["supersession_visibility_rate"]==1.0
    assert result["aggregate"]["G1"]["native_revision_rate"]==1.0
    assert result["aggregate"]["G2"]["challenge_precision"]==1.0
    assert result["aggregate"]["G2"]["exhausted_frontier_reopen_rate"]==0.0
    assert result["aggregate"]["G2"]["evidence_discovery_after_reopen_rate"]==1.0
    assert result["aggregate"]["G2"]["earned_resolution_rate"]==1.0

    # H1/H2 mirror transformation must not alter tie result.
    tie=[r for r in result["per_episode"]["G1"] if r["task_family"]=="exact_symmetric_tie"]
    assert len(tie)==2 and all(r["operative"] is None for r in tie)

    rejected("duplicate pair",lambda t,r:r["episodes"].append(copy.deepcopy(r["episodes"][0])))
    rejected("missing pair",lambda t,r:r["episodes"].pop())
    rejected("config mixing",lambda t,r:r["episodes"][0].__setitem__("configuration","G2"))
    rejected("oracle leakage",lambda t,r:r["episodes"][0].__setitem__("oracle",{"x":1}))
    rejected("expected leakage",lambda t,r:r["episodes"][0].__setitem__("expected_status","resolved"))
    rejected("runtime failure",lambda t,r:r["episodes"][0]["failures"].append("boom"))
    rejected("missing step",lambda t,r:r["episodes"][0]["steps"].pop())
    rejected("wrong ingestion",lambda t,r:r["episodes"][0]["steps"][0].__setitem__("ingested_evidence_ids",["wrong"]))
    rejected("cross episode ref",lambda t,r:r["episodes"][0]["steps"][0]["active_evidence_refs"].append("FOREIGN"))
    rejected("telemetry gap",lambda t,r:r["episodes"][0]["steps"][0]["telemetry_gaps"].append("gap"))
    def illegal_commit(t,r):
        ep=next(x for x in r["episodes"] if x["configuration"]=="C1")
        ep["steps"][-1]["final_state"]["committed_answer"]="H1"
    rejected("C1 commitment",illegal_commit)
    def contested_commit(t,r):
        ep=next(x for x in r["episodes"] if x["configuration"]=="G1" and x["steps"][-1]["final_state"]["committed_answer"] is not None)
        ep["steps"][-1]["final_state"]["epistemic_status"]="contested"
    rejected("commit under contested",contested_commit)

    # Score mode cannot be entered through ordinary evaluator API.
    tasks,raw=fixture(); raw["score_bearing"]=True; raw["score_bearing_authorized"]=True
    try: validate_inputs(tasks,raw)
    except EvaluationInputError: pass
    else: raise AssertionError("score-bearing input accepted without authorization")

    print("cycle6_v3_evaluator_adversarial=passed cases=13")

if __name__=="__main__":
    main()
