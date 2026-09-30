#!/usr/bin/env python3
"""Fail-closed evaluator candidate for EP-PROCESS-V3.

This is frozen and adversarially tested before any V3 score run. It may join
the frozen task oracle only when explicitly invoked by a future score workflow.
Cycle 6 itself uses synthetic fixtures only.
"""
from __future__ import annotations

import argparse, collections, json, math
from pathlib import Path
from typing import Any, Mapping, Sequence

from statistics_v3 import (
    BOOTSTRAP_RESAMPLES, BOOTSTRAP_SEED, holm_adjust,
    paired_binary_summary,
)

CONFIGS=("C0","G0E","C1","G1","G2")
ANSWER_CONFIGS={"C0","C1","G1","G2"}
COMMITMENT_CONFIGS={"G1","G2"}
TIE_FAMILIES={
    "exact_symmetric_tie","label_mirror_tie","node_id_permutation_tie",
    "target_insertion_permutation","evidence_order_permutation",
    "family_name_permutation",
}
HISTORY_FAMILIES={
    "valid_persistent_history","explicit_revocation","explicit_supersession",
    "refutation_no_replacement","refutation_insufficient_replacement",
    "refutation_sufficient_replacement","delayed_sufficient_replacement",
    "earned_revision","no_change_control",
}
CLEAR_NON_TIE_FAMILIES={
    "near_tie","duplicate_correlated_support","valid_persistent_history",
    "explicit_revocation","explicit_supersession","refutation_sufficient_replacement",
    "delayed_sufficient_replacement","correlated_majority_independent_minority",
    "earned_revision","earned_resolution","single_step_control","no_change_control",
}
FORBIDDEN_RAW_KEYS={
    "oracle","expected_terminal_operative","expected_terminal_committed",
    "expected_status","expected_abstention","correctness","accuracy",
    "claim_status","claim_ids","score_results",
}

class EvaluationInputError(ValueError):
    pass

def rate(n,d):
    return None if not d else n/d

def _scan_raw(value,path="root"):
    if isinstance(value,dict):
        for k,v in value.items():
            if k in FORBIDDEN_RAW_KEYS or k.startswith("expected_"):
                raise EvaluationInputError(f"oracle/outcome key leaked into raw at {path}.{k}")
            _scan_raw(v,f"{path}.{k}")
    elif isinstance(value,list):
        for i,v in enumerate(value): _scan_raw(v,f"{path}[{i}]")

def _all_task_evidence_ids(task):
    return {
        str(e["id"])
        for e in list(task["runtime"]["events"])+list(task["environment"]["latent_events"])
    }

def _event_map(task):
    return {
        str(e["id"]):e
        for e in list(task["runtime"]["events"])+list(task["environment"]["latent_events"])
    }

def _expected_active_refs(task,through_step):
    active={}
    for e in task["runtime"]["events"]:
        if int(e["step"])>through_step: break
        kind=str(e["kind"])
        if kind in {"support","refute","noop"}:
            active[str(e["id"])]=e
        elif kind=="revoke":
            for ref in e.get("revokes",[]): active.pop(str(ref),None)
        elif kind=="supersede":
            for ref in e.get("supersedes",[]): active.pop(str(ref),None)
            active[str(e["id"])]=e
    return active

def _expected_family_ids(task,through_step):
    active=_expected_active_refs(task,through_step)
    return sorted({str(e["canonical_family_id"]) for e in active.values()})

def _expected_lineage(task,through_step):
    active=_expected_active_refs(task,through_step)
    return sorted({
        str(dep)
        for e in active.values()
        for dep in e.get("depends_on",[])
    })

def _terminal(ep):
    if not ep.get("steps"): raise EvaluationInputError("episode has no steps")
    return ep["steps"][-1]["final_state"]

def _events(ep,kind=None):
    rows=[]
    for step in ep["steps"]:
        for event in step.get("native_events") or []:
            if kind is None or str(event.get("type") or "").lower()==kind:
                rows.append((int(step["step"]),event))
    return rows

def _dwm_rounds(ep):
    for step in ep["steps"]:
        for rr in step.get("recovery_rounds") or []:
            if rr.get("trigger")=="dwm_opposition":
                yield int(step["step"]),rr

def validate_inputs(tasks,raw,*,expected_configs=CONFIGS,allow_score_bearing=False):
    if raw.get("score_bearing") and not allow_score_bearing:
        raise EvaluationInputError("score-bearing raw not permitted in this invocation")
    if raw.get("score_bearing_authorized") and not allow_score_bearing:
        raise EvaluationInputError("score authorization not permitted")
    _scan_raw(raw)
    episodes=list(tasks.get("episodes") or [])
    task_ids=[str(t["id"]) for t in episodes]
    if len(task_ids)!=len(set(task_ids)) or not task_ids:
        raise EvaluationInputError("task ids must be unique and non-empty")
    configs=list(raw.get("configurations") or expected_configs)
    if tuple(configs)!=tuple(expected_configs):
        raise EvaluationInputError(f"configuration declaration mismatch: {configs}")
    raw_eps=list(raw.get("episodes") or [])
    pairs=[(str(e.get("episode_id")),str(e.get("configuration"))) for e in raw_eps]
    expected={(tid,c) for tid in task_ids for c in expected_configs}
    if set(pairs)!=expected or len(pairs)!=len(expected):
        raise EvaluationInputError("raw task×configuration matrix incomplete/duplicated/mixed")
    by_task={str(t["id"]):t for t in episodes}
    for ep in raw_eps:
        tid=str(ep["episode_id"]); cfg=str(ep["configuration"]); task=by_task[tid]
        if ep.get("failures"):
            raise EvaluationInputError(f"runtime failure present for {tid}/{cfg}")
        steps=list(ep.get("steps") or [])
        expected_events=list(task["runtime"]["events"])
        if len(steps)!=len(expected_events):
            raise EvaluationInputError(f"step count mismatch for {tid}/{cfg}")
        allowed=_all_task_evidence_ids(task)
        for step,event in zip(steps,expected_events):
            if int(step.get("step",-1))!=int(event["step"]):
                raise EvaluationInputError(f"step numbering mismatch for {tid}/{cfg}")
            if list(step.get("ingested_evidence_ids") or [])!=[str(event["id"])]:
                raise EvaluationInputError(f"wrong ingested evidence for {tid}/{cfg}")
            refs=[str(x) for x in step.get("active_evidence_refs") or []]
            if any(ref not in allowed for ref in refs):
                raise EvaluationInputError(f"cross-episode evidence leakage for {tid}/{cfg}")
            if step.get("telemetry_gaps"):
                raise EvaluationInputError(f"telemetry gap for {tid}/{cfg}")
            final=step.get("final_state") or {}
            if cfg in {"C0","G0E","C1"} and final.get("committed_answer") is not None:
                raise EvaluationInputError(f"{cfg} cannot emit committed answer")
            if final.get("committed_answer") is not None and final.get("epistemic_status") not in {"resolved","provisionally_resolved"}:
                raise EvaluationInputError(f"commitment under non-committing status {tid}/{cfg}")
    return True

def _episode_record(task,ep,cfg):
    terminal=_terminal(ep)
    oracle=task["oracle"]
    expected_op=oracle["expected_terminal_operative"]
    expected_commit=oracle["expected_terminal_committed"]
    rec={
        "episode_id":task["id"],"task_family":task["task_family"],
        "mirror_group":task.get("mirror_group"),"mirror_primary":task.get("mirror_primary"),
        "operative":terminal.get("operative_hypothesis"),
        "committed":terminal.get("committed_answer"),
        "status":terminal.get("epistemic_status"),
        "operative_eligible":expected_op is not None,
        "operative_correct":expected_op is not None and terminal.get("operative_hypothesis")==expected_op,
        "commitment_coverage":terminal.get("committed_answer") is not None,
        "commitment_correct":terminal.get("committed_answer") is not None and terminal.get("committed_answer")==expected_commit,
        "correct_commitment_yield":expected_commit is not None and terminal.get("committed_answer")==expected_commit,
        "false_commitment":terminal.get("committed_answer") is not None and terminal.get("committed_answer")!=expected_commit,
        "abstention_eligible":expected_commit is None,
        "appropriate_abstention":expected_commit is None and terminal.get("committed_answer") is None,
        "inappropriate_abstention":expected_commit is not None and terminal.get("committed_answer") is None,
    }

    # Persistence/mechanical surfaces.
    exact=family_exact=lineage_exact=0
    for step in ep["steps"]:
        n=int(step["step"])
        expected_refs=sorted(_expected_active_refs(task,n))
        actual_refs=sorted(str(x) for x in step.get("active_evidence_refs") or [])
        exact += actual_refs==expected_refs
        expected_families=_expected_family_ids(task,n)
        actual_families=sorted(str(x) for x in (step["final_state"].get("evidence_family_ids") or []))
        family_exact += actual_families==expected_families
        expected_lineage=_expected_lineage(task,n)
        actual_lineage=step["final_state"].get("dependency_lineage_ids")
        if actual_lineage is None:
            lineage_exact += False
        else:
            lineage_exact += sorted(str(x) for x in actual_lineage)==expected_lineage
    rec.update({
        "retention_exact":exact,"retention_total":len(ep["steps"]),
        "family_exact":family_exact,"family_total":len(ep["steps"]),
        "lineage_exact":lineage_exact,"lineage_total":len(ep["steps"]),
    })

    # Revocation/supersession visibility.
    rev_total=rev_visible=sup_total=sup_visible=invalid_leak=invalid_total=0
    for event in task["runtime"]["events"]:
        n=int(event["step"])
        step=ep["steps"][n-1]
        active=set(str(x) for x in step.get("active_evidence_refs") or [])
        if event["kind"]=="revoke":
            for ref in event.get("revokes",[]):
                rev_total+=1; invalid_total+=1
                rev_visible += str(ref) not in active
                invalid_leak += str(ref) in active
        if event["kind"]=="supersede":
            for ref in event.get("supersedes",[]):
                sup_total+=1; invalid_total+=1
                sup_visible += str(ref) not in active
                invalid_leak += str(ref) in active
    rec.update({"rev_total":rev_total,"rev_visible":rev_visible,
                "sup_total":sup_total,"sup_visible":sup_visible,
                "invalid_total":invalid_total,"invalid_leak":invalid_leak})

    # Native revision.
    revision_events=_events(ep,"revision")
    expected_revision=oracle.get("expected_revision")
    rec["revision_required"]=bool(oracle.get("revision_required"))
    rec["native_revision"]=False
    rec["revision_receipt_complete"]=False
    rec["revision_latency"]=None
    if expected_revision:
        for step_no,event in revision_events:
            complete=("previous_hypothesis" in event and "hypothesis" in event)
            earned_step=int(expected_revision["replacement_earned_step"])
            if (complete and step_no>=earned_step and event.get("previous_hypothesis")==expected_revision["from"] and event.get("hypothesis")==expected_revision["to"]:
                rec["native_revision"]=True
                rec["revision_receipt_complete"]=True
                rec["revision_latency"]=step_no-earned_step
                break
    rec["false_revision"]=(not rec["revision_required"]) and bool(revision_events)

    # Resolution.
    rec["resolution_eligible"]=bool(oracle.get("resolution_eligible"))
    rec["earned_resolution"]=(
        rec["resolution_eligible"] and terminal.get("epistemic_status")=="resolved"
        and terminal.get("committed_answer")==expected_commit and expected_commit is not None
    )
    rec["resolved"]=terminal.get("epistemic_status")=="resolved"
    rec["false_convergence"]=rec["resolved"] and not rec["earned_resolution"]

    # DWM process.
    warranted=set(int(x) for x in oracle.get("challenge_warranted_steps") or [])
    challenges=_events(ep,"challenge"); reopens=_events(ep,"reopen")
    rec["challenge_count"]=len(challenges)
    rec["challenge_correct"]=sum(step in warranted for step,_ in challenges)
    rec["warranted_challenge_steps"]=len(warranted)
    rec["warranted_challenge_observed"]=len({step for step,_ in challenges if step in warranted})
    positive_opportunity=bool(oracle.get("expected_expansion_opportunity_class"))
    rec["reopen_count"]=len(reopens)
    rec["unnecessary_reopen"]=len(reopens) if not positive_opportunity else 0
    rec["reopen_opportunity_correct"]=len(reopens) if positive_opportunity else 0
    rec["exhausted_episode_reopened"]=task["task_family"]=="dwm_exhausted_frontier_negative" and bool(reopens)
    useful=frontier=rank=decision=0
    for _,rr in _dwm_rounds(ep):
        changed=any(bool(rr.get(k)) for k in (
            "frontier_changed","rank_changed","operative_hypothesis_changed",
            "status_changed","committed_answer_changed"))
        useful+=changed; frontier+=bool(rr.get("frontier_changed"))
        rank+=bool(rr.get("rank_changed"))
        decision+=bool(rr.get("operative_hypothesis_changed") or rr.get("status_changed") or rr.get("committed_answer_changed"))
    rec.update({"useful_reopen":useful,"frontier_change":frontier,"rank_change":rank,"decision_change":decision})
    latent=set(str(x) for x in oracle.get("latent_evidence_ids") or [])
    discovered=any(latent & set(str(x) for x in step.get("active_evidence_refs") or []) for step in ep["steps"])
    rec["latent_episode"]=bool(latent)
    rec["latent_discovered_after_reopen"]=bool(latent and reopens and discovered)

    # Policy/tie conformance.
    rec["exact_tie_unique_selection"]=task["task_family"] in TIE_FAMILIES and terminal.get("operative_hypothesis") is not None
    rec["exact_tie_abstention"]=task["task_family"] in TIE_FAMILIES and terminal.get("committed_answer") is None
    rec["near_tie_correct"]=task["task_family"]=="near_tie" and terminal.get("operative_hypothesis")==expected_op
    rec["replacement_policy_conform"]=(
        task["task_family"]!="refutation_insufficient_replacement"
        or (terminal.get("operative_hypothesis") is None and terminal.get("committed_answer") is None and terminal.get("epistemic_status")=="contested")
    )
    return rec

def _mirror_transform(value):
    if value=="H1": return "H2"
    if value=="H2": return "H1"
    return value

def _tie_invariance(records):
    groups=collections.defaultdict(list)
    for r in records:
        if r["task_family"] in TIE_FAMILIES:
            groups[r["mirror_group"]].append(r)
    ok=total=0
    for pair in groups.values():
        if len(pair)!=2: continue
        a,b=sorted(pair,key=lambda x:x["mirror_primary"])
        total+=1
        transformed=(_mirror_transform(a["operative"]),_mirror_transform(a["committed"]),a["status"])
        ok += transformed==(b["operative"],b["committed"],b["status"])
    return rate(ok,total),ok,total

def _aggregate(records,cfg):
    agg={}
    eligible=[r for r in records if r["operative_eligible"]]
    agg["operative_hypothesis_accuracy"]=rate(sum(r["operative_correct"] for r in eligible),len(eligible))
    if cfg in COMMITMENT_CONFIGS:
        agg["commitment_coverage"]=rate(sum(r["commitment_coverage"] for r in records),len(records))
        committed=[r for r in records if r["commitment_coverage"]]
        agg["committed_accuracy"]=rate(sum(r["commitment_correct"] for r in committed),len(committed))
        agg["correct_commitment_yield"]=rate(sum(r["correct_commitment_yield"] for r in records),len(records))
        agg["false_commitment_incidence"]=rate(sum(r["false_commitment"] for r in records),len(records))
        abst=[r for r in records if r["abstention_eligible"]]
        need=[r for r in records if not r["abstention_eligible"]]
        agg["appropriate_abstention_rate"]=rate(sum(r["appropriate_abstention"] for r in abst),len(abst))
        agg["inappropriate_abstention_rate"]=rate(sum(r["inappropriate_abstention"] for r in need),len(need))
    agg["evidence_retention_exactness"]=rate(sum(r["retention_exact"] for r in records),sum(r["retention_total"] for r in records))
    agg["evidence_family_identity_exactness"]=rate(sum(r["family_exact"] for r in records),sum(r["family_total"] for r in records))
    agg["dependency_lineage_exactness"]=rate(sum(r["lineage_exact"] for r in records),sum(r["lineage_total"] for r in records))
    agg["revocation_visibility_rate"]=rate(sum(r["rev_visible"] for r in records),sum(r["rev_total"] for r in records))
    agg["supersession_visibility_rate"]=rate(sum(r["sup_visible"] for r in records),sum(r["sup_total"] for r in records))
    agg["invalidated_active_support_leakage_rate"]=rate(sum(r["invalid_leak"] for r in records),sum(r["invalid_total"] for r in records))
    req=[r for r in records if r["revision_required"]]
    neg=[r for r in records if not r["revision_required"]]
    agg["native_revision_rate"]=rate(sum(r["native_revision"] for r in req),len(req))
    agg["false_revision_rate"]=rate(sum(r["false_revision"] for r in neg),len(neg))
    agg["native_revision_receipt_completeness"]=rate(sum(r["revision_receipt_complete"] for r in req),len(req))
    lat=sorted(r["revision_latency"] for r in req if r["revision_latency"] is not None)
    agg["revision_latency_steps_p95"]=None if not lat else lat[min(len(lat)-1,math.ceil(.95*len(lat))-1)]
    res=[r for r in records if r["resolution_eligible"]]
    resolved=[r for r in records if r["resolved"]]
    agg["earned_resolution_rate"]=rate(sum(r["earned_resolution"] for r in res),len(res))
    agg["false_convergence_rate"]=rate(sum(r["false_convergence"] for r in resolved),len(resolved))
    if cfg=="G2":
        ch=sum(r["challenge_count"] for r in records)
        warranted=sum(r["warranted_challenge_steps"] for r in records)
        agg["challenge_precision"]=rate(sum(r["challenge_correct"] for r in records),ch)
        agg["challenge_recall"]=rate(sum(r["warranted_challenge_observed"] for r in records),warranted)
        agg["unnecessary_challenge_rate"]=None if not ch else 1-(sum(r["challenge_correct"] for r in records)/ch)
        reopens=sum(r["reopen_count"] for r in records)
        agg["unnecessary_reopen_rate"]=rate(sum(r["unnecessary_reopen"] for r in records),reopens)
        agg["reopen_opportunity_precision"]=rate(sum(r["reopen_opportunity_correct"] for r in records),reopens)
        exhausted=[r for r in records if r["task_family"]=="dwm_exhausted_frontier_negative"]
        agg["exhausted_frontier_reopen_rate"]=rate(sum(r["exhausted_episode_reopened"] for r in exhausted),len(exhausted))
        agg["reopen_usefulness_rate"]=rate(sum(r["useful_reopen"] for r in records),reopens)
        latent_reopens=sum(r["reopen_count"] for r in records if r["latent_episode"])
        agg["evidence_discovery_after_reopen_rate"]=rate(sum(r["latent_discovered_after_reopen"] for r in records),latent_reopens)
        agg["frontier_change_after_reopen_rate"]=rate(sum(r["frontier_change"] for r in records),reopens)
        agg["ranking_change_after_reopen_rate"]=rate(sum(r["rank_change"] for r in records),reopens)
        agg["decision_change_after_reopen_rate"]=rate(sum(r["decision_change"] for r in records),reopens)
    tie,_,_= _tie_invariance(records)
    agg["tie_invariance_rate"]=tie
    ties=[r for r in records if r["task_family"] in TIE_FAMILIES]
    agg["exact_tie_unique_selection_rate"]=rate(sum(r["exact_tie_unique_selection"] for r in ties),len(ties))
    agg["appropriate_abstention_rate_on_exact_ties"]=rate(sum(r["exact_tie_abstention"] for r in ties),len(ties))
    near=[r for r in records if r["task_family"]=="near_tie"]
    agg["near_tie_correct_selection_rate"]=rate(sum(r["near_tie_correct"] for r in near),len(near))
    repl=[r for r in records if r["task_family"]=="refutation_insufficient_replacement"]
    agg["incumbent_replacement_policy_conformity"]=rate(sum(r["replacement_policy_conform"] for r in repl),len(repl))
    return agg

def _paired(by_cfg,control,treatment,predicate=lambda r:True,key="operative_correct"):
    c={r["episode_id"]:r for r in by_cfg[control] if predicate(r)}
    t={r["episode_id"]:r for r in by_cfg[treatment] if predicate(r)}
    if set(c)!=set(t) or not c:
        raise EvaluationInputError(f"paired population mismatch {control}->{treatment}")
    ids=sorted(c)
    return [bool(c[i][key]) for i in ids],[bool(t[i][key]) for i in ids]

def evaluate(tasks,raw,*,expected_configs=CONFIGS,allow_score_bearing=False):
    validate_inputs(tasks,raw,expected_configs=expected_configs,allow_score_bearing=allow_score_bearing)
    task_by={str(t["id"]):t for t in tasks["episodes"]}
    ep_by={(str(e["episode_id"]),str(e["configuration"])):e for e in raw["episodes"]}
    by_cfg={c:[] for c in expected_configs}
    for cfg in expected_configs:
        for tid in sorted(task_by):
            by_cfg[cfg].append(_episode_record(task_by[tid],ep_by[(tid,cfg)],cfg))
    aggregate={cfg:_aggregate(by_cfg[cfg],cfg) for cfg in expected_configs}

    gate={}
    if {"C0","C1"}<=set(expected_configs):
        c,t=_paired(by_cfg,"C0","C1",lambda r:r["task_family"] in HISTORY_FAMILIES)
        s=paired_binary_summary(c,t)
        gate["history_dependent_operative_accuracy_delta"]=s["delta_treatment_minus_control"]
        gate["history_dependent_delta_ci_low"]=s["ci_low"]
        gate["history_dependent_Holm_p"]=s["exact_mcnemar"]["p_value_two_sided_exact"]
        c,t=_paired(by_cfg,"C0","C1",lambda r:r["task_family"]=="single_step_control")
        gate["single_step_control_accuracy_delta"]=paired_binary_summary(c,t)["delta_treatment_minus_control"]
    if {"C1","G1"}<=set(expected_configs):
        c,t=_paired(by_cfg,"C1","G1",lambda r:r["task_family"] in CLEAR_NON_TIE_FAMILIES)
        gate["clear_non_tie_accuracy_delta"]=paired_binary_summary(c,t)["delta_treatment_minus_control"]
    if "G1" in aggregate:
        gate.update({
            "tie_invariance_rate":aggregate["G1"]["tie_invariance_rate"],
            "exact_tie_unique_selection_rate":aggregate["G1"]["exact_tie_unique_selection_rate"],
            "appropriate_abstention_rate_on_exact_ties":aggregate["G1"]["appropriate_abstention_rate_on_exact_ties"],
            "near_tie_correct_selection_rate":aggregate["G1"]["near_tie_correct_selection_rate"],
            "incumbent_replacement_policy_conformity":aggregate["G1"]["incumbent_replacement_policy_conformity"],
            "native_revision_rate":aggregate["G1"]["native_revision_rate"],
            "false_revision_rate":aggregate["G1"]["false_revision_rate"],
            "revision_latency_steps_p95":aggregate["G1"]["revision_latency_steps_p95"],
            "native_revision_receipt_completeness":aggregate["G1"]["native_revision_receipt_completeness"],
        })
    if "G0E" in aggregate:
        gate.update({
            "evidence_retention_exactness":aggregate["G0E"]["evidence_retention_exactness"],
            "evidence_family_identity_exactness":aggregate["G0E"]["evidence_family_identity_exactness"],
            "dependency_lineage_exactness":aggregate["G0E"]["dependency_lineage_exactness"],
            "cross_episode_leakage_rate":0.0,
            "invalidated_active_support_leakage_rate":aggregate["G0E"]["invalidated_active_support_leakage_rate"],
            "revocation_visibility_rate":aggregate["G0E"]["revocation_visibility_rate"],
            "supersession_visibility_rate":aggregate["G0E"]["supersession_visibility_rate"],
        })
    if "G2" in aggregate:
        for k in (
            "challenge_precision","challenge_recall","unnecessary_challenge_rate",
            "unnecessary_reopen_rate","reopen_opportunity_precision",
            "exhausted_frontier_reopen_rate","reopen_usefulness_rate",
            "evidence_discovery_after_reopen_rate","earned_resolution_rate",
            "false_convergence_rate",
        ): gate[k]=aggregate["G2"].get(k)

    comparisons={}
    if {"G1","G2"}<=set(expected_configs):
        ids=sorted({r["episode_id"] for r in by_cfg["G1"]})
        g1={r["episode_id"]:r for r in by_cfg["G1"]}; g2={r["episode_id"]:r for r in by_cfg["G2"]}
        false=paired_binary_summary([g1[i]["false_commitment"] for i in ids],[g2[i]["false_commitment"] for i in ids])
        gate["false_commitment_incidence_delta"]=false["delta_treatment_minus_control"]
        gate["false_commitment_incidence_delta_ci_high"]=false["ci_high"]
        gate["false_commitment_incidence_Holm_p"]=holm_adjust({"false_commitment":false["exact_mcnemar"]["p_value_two_sided_exact"]})["false_commitment"]
        gate["G2_false_commitment_incidence"]=aggregate["G2"]["false_commitment_incidence"]
        gate["appropriate_abstention_delta"]=(aggregate["G2"]["appropriate_abstention_rate"] or 0)-(aggregate["G1"]["appropriate_abstention_rate"] or 0)
        gate["commitment_coverage_delta"]=(aggregate["G2"]["commitment_coverage"] or 0)-(aggregate["G1"]["commitment_coverage"] or 0)
        gate["correct_commitment_yield_delta"]=(aggregate["G2"]["correct_commitment_yield"] or 0)-(aggregate["G1"]["correct_commitment_yield"] or 0)
        gate["operative_hypothesis_accuracy_delta"]=(aggregate["G2"]["operative_hypothesis_accuracy"] or 0)-(aggregate["G1"]["operative_hypothesis_accuracy"] or 0)
        gate["committed_accuracy_delta"]=(aggregate["G2"]["committed_accuracy"] or 0)-(aggregate["G1"]["committed_accuracy"] or 0)
        comparisons["G1->G2_false_commitment"]=false

    return {"aggregate":aggregate,"per_episode":by_cfg,"comparisons":comparisons,"gate_metrics":gate}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--tasks",required=True); p.add_argument("--raw",required=True)
    p.add_argument("--out",required=True)
    p.add_argument("--mode",choices=("unscored-evaluator-test","score"),required=True)
    args=p.parse_args()
    tasks=json.loads(Path(args.tasks).read_text()); raw=json.loads(Path(args.raw).read_text())
    if args.mode=="score":
        raise SystemExit("score mode is intentionally disabled until a later score-authorization manifest exists")
    result=evaluate(tasks,raw,allow_score_bearing=False)
    out={"schema":"epistemic-process-v3-evaluator-v1","mode":args.mode,
         "score_bearing":False,"bootstrap_seed":BOOTSTRAP_SEED,
         "bootstrap_resamples":BOOTSTRAP_RESAMPLES,**result}
    Path(args.out).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")

if __name__=="__main__":
    main()
