#!/usr/bin/env python3
"""Mechanism-level UNSCORED analysis for Epistemic Process V2."""
from __future__ import annotations
import argparse, json
from pathlib import Path
from typing import Any

CONFIGS=("B0","G0","G1","G2")

def load(p: Path)->Any:
    return json.loads(p.read_text(encoding="utf-8"))

def rate(n,d):
    return None if not d else n/d

def node_label(ep,node):
    inv={int(v):k for k,v in (ep.get("hypothesis_nodes") or {}).items()}
    return inv.get(int(node or 0))

def temporal_episode(task, raw_ep, receipt):
    oracle=task.get("temporal_oracle") or {}
    mode=oracle.get("mode"); trigger=oracle.get("trigger_step")
    observations=[]
    for step in raw_ep.get("steps") or []:
        t=step.get("temporal_observation") or {}
        if not t.get("previous_available"): continue
        observations.append({
            "step":int(step.get("step",0)),
            "changed":bool(t.get("changed")),
            "from":node_label(raw_ep,t.get("previous_hypothesis_node")),
            "to":node_label(raw_ep,t.get("current_hypothesis_node")),
            "previous_has_answer":bool(t.get("previous_has_answer")),
            "current_has_answer":bool(t.get("current_has_answer")),
        })
    changed=[x for x in observations if x["changed"]]
    post=[x for x in changed if trigger is None or x["step"]>=int(trigger)]
    if mode=="no_change":
        satisfied=len(changed)==0
    elif mode in {"to_H1","to_H2"}:
        target=mode.split("_",1)[1]
        satisfied=any(x["to"]==target and x["current_has_answer"] for x in post)
    elif mode=="to_abstain":
        decisions=receipt.get("decisions") or []
        satisfied=any(
            int(d.get("step",0)) >= int(trigger or 0)
            and d.get("status")=="abstain"
            and d.get("hypothesis") is None
            for d in decisions
        )
    else:
        satisfied=False
    return {
        "mode":mode,"trigger_step":trigger,"satisfied":satisfied,
        "changes":changed,"change_count":len(changed),
    }

def g0_integrity(tasks, raw_eps):
    numer=denom=family_n=family_d=0
    rows=[]
    by_task={x["id"]:x for x in tasks}
    for ep in raw_eps:
        task=by_task[ep["episode_id"]]
        events={int(e["step"]):e for e in task["events"]}
        active=set()
        metadata=ep.get("evidence_metadata") or {}
        for step in ep.get("steps") or []:
            ev=events[int(step["step"])]
            for ref in ev.get("revokes") or []: active.discard(str(ref))
            if ev.get("kind")!="noise": active.add(str(ev["id"]))
            observed=set(str(x) for x in ((step.get("decision") or {}).get("evidence_refs") or []))
            numer += len(active & observed); denom += len(active)
            for ref in observed:
                family_d += 1
                if isinstance(metadata.get(ref),dict) and metadata[ref].get("family"):
                    family_n += 1
            rows.append({"episode_id":ep["episode_id"],"step":step["step"],
                         "expected_active_refs":len(active),"observed_refs":len(observed),
                         "matched_refs":len(active & observed)})
    return {
        "evidence_reference_recall":{"numerator":numer,"denominator":denom,"rate":rate(numer,denom)},
        "evidence_family_integrity":{"numerator":family_n,"denominator":family_d,"rate":rate(family_n,family_d)},
        "rows":rows,
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--tasks",required=True)
    ap.add_argument("--artifacts-dir",required=True)
    ap.add_argument("--out",required=True)
    args=ap.parse_args()
    root=Path(args.artifacts_dir); tasks_doc=load(Path(args.tasks)); tasks=tasks_doc["episodes"]
    task_by_id={x["id"]:x for x in tasks}
    report={"schema":"epistemic-process-v2-score-analysis" if args.mode=="score" else "epistemic-process-v2-unscored-analysis","score_bearing":args.mode=="score",
            "claim_interpretation":"not_performed_by_script","configurations":{},"per_stratum":{}}
    raw_by={}; receipts_by={}; eval_by={}
    for cfg in CONFIGS:
        raw=load(root/f"raw-{cfg}.json"); receipts=load(root/f"receipts-{cfg}.json"); ev=load(root/f"scores-{cfg}.json")
        raw_eps=raw.get("episodes") or []; raw_by[cfg]=raw_eps
        rmap={x["episode_id"]:x for x in receipts}; receipts_by[cfg]=rmap; eval_by[cfg]=ev
        temporal=[]
        revisions=challenges=reopens=0
        effective_calls=expanded_calls=0
        execution_seconds=0.0; visited=expansions=edges=0
        failures=0
        for ep in raw_eps:
            receipt=rmap[ep["episode_id"]]
            if ep.get("failures") or receipt.get("adapter_failure"): failures+=1
            temporal.append({"episode_id":ep["episode_id"],**temporal_episode(task_by_id[ep["episode_id"]],ep,receipt)})
            revisions += sum(1 for t in receipt.get("transitions") or [] if t.get("type")=="revise")
            reopens += sum(1 for t in receipt.get("transitions") or [] if t.get("type")=="reopen")
            challenges += len(receipt.get("challenges") or [])
            execution_seconds += float(ep.get("execution_seconds") or 0.0)
            for x in ep.get("runtime_receipts") or []:
                rounds=int(x.get("expansion_rounds") or 0)
                expansions += rounds; visited += int(x.get("visited_states") or 0); edges += int(x.get("evidence_edge_count") or 0)
                if rounds>0:
                    expanded_calls+=1
                    if int(x.get("frontier_progress_rounds") or 0)>0 or x.get("initial_bundle_hash")!=x.get("final_bundle_hash"):
                        effective_calls+=1
        positive=[x for x in temporal if x["mode"]!="no_change"]
        negative=[x for x in temporal if x["mode"]=="no_change"]
        agg=ev["aggregate"]
        cfg_report={
            "episodes":len(raw_eps),"failures":failures,
            "temporal_change":{
                "positive_oracle_satisfied":sum(1 for x in positive if x["satisfied"]),
                "positive_oracle_episodes":len(positive),
                "sensitivity":rate(sum(1 for x in positive if x["satisfied"]),len(positive)),
                "negative_controls_without_change":sum(1 for x in negative if x["satisfied"]),
                "negative_control_episodes":len(negative),
                "specificity":rate(sum(1 for x in negative if x["satisfied"]),len(negative)),
                "observed_change_events":sum(x["change_count"] for x in temporal),
                "per_episode":temporal,
            },
            "native_events":{"revision_transitions":revisions,"challenge_events":challenges,"reopen_transitions":reopens},
            "reopen_effectiveness":{
                "expanded_runtime_calls":expanded_calls,"effective_runtime_calls":effective_calls,
                "effective_call_rate":rate(effective_calls,expanded_calls),
                "note":"Effective means bundle/frontier changed during a runtime call; reported separately from answer change."
            },
            "terminal":{
                "coverage_rate":agg["selective_coverage"]["coverage_rate"],
                "abstention_rate":agg["selective_coverage"]["abstention_rate"],
                "covered_accuracy":agg["selective_coverage"]["covered_accuracy"],
                "overall_terminal_accuracy":agg["selective_coverage"]["overall_terminal_accuracy"],
                "false_convergence_rate":agg["false_convergence_rate"]["rate"],
                "refutation_response_rate":agg["refutation_response_rate"]["rate"],
                "receipt_completeness_rate":agg["receipt_completeness"]["rate"],
                "g0_terminal_accuracy_is_primary":False if cfg=="G0" else None,
            },
            "cost":{"execution_seconds_total":execution_seconds,
                    "execution_seconds_mean":rate(execution_seconds,len(raw_eps)),
                    "visited_states_total":visited,"expansion_rounds_total":expansions,
                    "evidence_edge_count_total":edges},
        }
        if cfg=="G0": cfg_report["graphene_evidence_integrity"]=g0_integrity(tasks,raw_eps)
        report["configurations"][cfg]=cfg_report

    for stratum in sorted({x["stratum"] for x in tasks}):
        ids={x["id"] for x in tasks if x["stratum"]==stratum}
        row={}
        for cfg in CONFIGS:
            evrows=[x for x in eval_by[cfg]["episodes"] if x["episode_id"] in ids and not x.get("excluded_from_primary_metrics")]
            correct=sum(1 for x in evrows if (x.get("selective_coverage") or {}).get("terminal_correct"))
            temporal=[x for x in report["configurations"][cfg]["temporal_change"]["per_episode"] if x["episode_id"] in ids]
            row[cfg]={"episodes":len(evrows),"terminal_correct":correct,
                      "terminal_accuracy":rate(correct,len(evrows)),
                      "temporal_oracle_satisfied":sum(1 for x in temporal if x["satisfied"]),
                      "temporal_oracle_rate":rate(sum(1 for x in temporal if x["satisfied"]),len(temporal))}
        report["per_stratum"][stratum]=row

    Path(args.out).parent.mkdir(parents=True,exist_ok=True)
    Path(args.out).write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps({
        "score_bearing":args.mode=="score,
        "episodes_per_configuration":{c:report["configurations"][c]["episodes"] for c in CONFIGS},
        "failures":{c:report["configurations"][c]["failures"] for c in CONFIGS},
        "native_revisions":{c:report["configurations"][c]["native_events"]["revision_transitions"] for c in CONFIGS},
        "g2_challenges":report["configurations"]["G2"]["native_events"]["challenge_events"],
        "g2_reopens":report["configurations"]["G2"]["native_events"]["reopen_transitions"],
    },sort_keys=True))

if __name__=="__main__":
    main()
