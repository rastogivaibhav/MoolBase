#!/usr/bin/env python3
"""Strict scoring of actual native C++ runtime on synthetic labelled fault injections.

Never passed to runtime. No assertion of independent causal validation.
"""
import argparse,json,math
from pathlib import Path

ARMS=("G0","G1","G2")
STAGES=("T0","T1")
def evaluate(records,truth):
    if truth.get("experiment")!="MOOL-EVIDENCE-LEARNING-V12-001":
        raise ValueError("not original intervention contract")
    if truth.get("presented_to_reasoner") is not False or truth.get("external_adjudication") is not False:
        raise ValueError("designer fixture falsely claimed independent adjudication")
    labeled={x["case_id"]:x["injected_cause"] for x in truth["incidents"]}
    if set(labeled)!={"incident-101","incident-102","incident-103"}:
        raise ValueError("wrong hidden synthetic fault population")
    if any(x not in {"H-A","H-B","H-C"} for x in labeled.values()):
        raise ValueError("invalid hypothetical cause label")
    if len(records)!=18:raise ValueError("must execute all 3x2x3 native conditions")
    seen={}
    for x in records:
        key=(x["case_id"],x["stage"],x["arm"])
        if (x.get("experiment")!="MOOL-EVIDENCE-LEARNING-V12-001" or
            x["case_id"] not in labeled or x["stage"] not in STAGES or x["arm"] not in ARMS or
            key in seen):
            raise ValueError("invalid or duplicate real-native condition")
        seen[key]=x
        if x["reasoning_mode"]!="Theoretical":
            raise ValueError("unauthenticated data passed to empirical mode")
        if (x["generic_telemetry_quarantined"] is not True or
            x["source_authenticated"] is not False or
            x["scientific_truth_verified"] is not False or
            x["independent_empirical_outcome_observed"] is not False or
            x["model_world_updated"] is not False):
            raise ValueError("untrusted synthetic source promoted into a fact")
        if x["evidence_authority"]!="unverified_synthetic":
            raise ValueError("false evidence authority")
        if x["incident_hypotheses"]!=3 or x["native_graph_edges"]!=(4 if x["stage"]=="T0" else 6):
            raise ValueError("not identical incident population and evidence schedule")
        if x["has_proposed_answer"] and x["selected_hypothesis"] not in {"H-A","H-B","H-C"}:
            raise ValueError("invalid proposed hypothesis")
        if not x["has_proposed_answer"] and x["selected_hypothesis"]!="none":
            raise ValueError("invented answer despite abstention")
        if x["actual_dwm_opposition"]!=(x["arm"]=="G2"):
            raise ValueError("not genuine DWM ablation")
        if not (math.isfinite(x["elapsed_ms"]) and x["elapsed_ms"]>=0):
            raise ValueError("unmeasured runtime latency")
    if set(seen)!={(cid,t,a) for cid in labeled for t in STAGES for a in ARMS}:
        raise ValueError("native execution matrix missing conditions")
    result=[]
    for cid in sorted(labeled):
        for stage in STAGES:
            for arm in ARMS:
                x=seen[(cid,stage,arm)]
                proposed=x["has_proposed_answer"]
                result.append({
                    "case_id":cid,"time":stage,"arm":arm,
                    "status":x["status"],
                    "selected_hypothesis":x["selected_hypothesis"],
                    "declared_synthetic_intervention":labeled[cid],
                    "has_proposed_answer":proposed,
                    "proposed_matches_fixture_label":bool(proposed and x["selected_hypothesis"]==labeled[cid]),
                    "proposed_wrong_fixture_label":bool(proposed and x["selected_hypothesis"]!=labeled[cid]),
                    "dwm_challenge":x["dialectical_challenge"],
                    "reopen_requested":x["reopen_requested"],
                    "elapsed_ms":x["elapsed_ms"]
                })
    arms={}
    for arm in ARMS:
        rr=[r for r in result if r["arm"]==arm]
        paired=[]
        for cid in sorted(labeled):
            before=seen[(cid,"T0",arm)];after=seen[(cid,"T1",arm)]
            paired.append({
                "case_id":cid,
                "initial":before["selected_hypothesis"],
                "after_new_evidence":after["selected_hypothesis"],
                "any_change":before["selected_hypothesis"]!=after["selected_hypothesis"],
                "initial_answer":before["has_proposed_answer"],
                "final_answer":after["has_proposed_answer"],
                "revised_to_fixture_preferred":bool(after["has_proposed_answer"] and
                        after["selected_hypothesis"]==labeled[cid] and
                        before["selected_hypothesis"]!=labeled[cid])
            })
        arms[arm]={
            "n":len(rr),"proposed_answer_count":sum(x["has_proposed_answer"] for x in rr),
            "proposed_match_synthetic_label":sum(x["proposed_matches_fixture_label"] for x in rr),
            "proposed_wrong_synthetic_label":sum(x["proposed_wrong_fixture_label"] for x in rr),
            "challenge_n":sum(x["dwm_challenge"] for x in rr),
            "reopen_n":sum(x["reopen_requested"] for x in rr),
            "mean_ms":sum(x["elapsed_ms"] for x in rr)/len(rr),
            "changed_selected_hypothesis_n":sum(p["any_change"] for p in paired),
            "revised_to_fixture_preferred_n":sum(p["revised_to_fixture_preferred"] for p in paired),
            "pairs":paired
        }
    return {
        "experiment":"MOOL-EVIDENCE-LEARNING-V12-001",
        "lab_phase":"PHASE_0_SYNTHETIC_NATIVE_VERTICAL_SLICE",
        "native_calls":18,
        "actual_llm_only_baseline":"NOT_RUN",
        "actual_llm_retrieval_baseline":"NOT_RUN",
        "empirical_accuracy":"NOT_MEASURABLE",
        "independently_adjudicated_outcomes":0,
        "external_source_authenticated":False,
        "learning_on_unseen_cases":"NOT_RUN",
        "new_scientific_facts_verified":0,
        "native_ablation_arms":arms,
        "case_receipts":result,
        "limitations":[
          "The three injected-cause labels are authored by the same project as the fixtures.",
          "The native run is Theoretical and uses Hypothetical/unverified edges only.",
          "The model-world state is deliberately not updated: no evidence of learning.",
          "G0 is a graph evidence projection; not equivalent to an LLM-only baseline.",
          "Same synthetic graph per arm does not imply comparable inference budgets to LLMs."
        ]
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--native",required=True)
    p.add_argument("--labels",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    raw=[json.loads(s) for s in Path(a.native).read_text().splitlines() if s.strip()]
    labels=json.loads(Path(a.labels).read_text())
    output=evaluate(raw,labels)
    Path(a.out).write_text(json.dumps(output,indent=2,sort_keys=True)+"\n")
    for k,v in output["native_ablation_arms"].items():
        print(f"v12_native_{k}_scored proposed={v['proposed_answer_count']}/6 "
              f"matched_synthetic_label={v['proposed_match_synthetic_label']}/6 "
              f"changed_after_evidence={v['changed_selected_hypothesis_n']}/3 "
              f"challenges={v['challenge_n']}/6")
    print("v12_source_and_outcome_boundary=PASS empirical_science=NOT_MEASURABLE")
if __name__=="__main__":main()
