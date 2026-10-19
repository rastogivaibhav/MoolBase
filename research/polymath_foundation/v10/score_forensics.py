#!/usr/bin/env python3
"""F10 read-only evidence admission forensics & separately labelled synthetic outcomes."""
import argparse,hashlib,json,math
from pathlib import Path

STAGES=("baseline","contradiction","revision")
ARMS=("G0","G1","G2")
COUNTS=("retrieved_target_fibers","retrieved_fiber_paths",
    "path_edge_count","contradiction_edges_in_paths",
    "support_role_paths","opposition_role_paths","noise_role_paths",
    "unknown_role_paths","support_eligible_paths","opposition_eligible_paths",
    "hypothetical_paths","nonoperative_paths","critical_evidence_missing_paths",
    "opposition_correlation_groups","opposition_representatives_eligible")

def evaluate(rows,labels):
    if len(rows)!=9:raise ValueError("requires 9 actual native receipts")
    if labels.get("external_adjudication") is not False or labels.get("may_be_presented_to_reasoner") is not False:
        raise ValueError("untrusted adjudication packet mislabelled")
    labelled={x["stage"]:x["preferred_hypothesis"] for x in labels["labels"]}
    if set(labelled)!=set(STAGES) or any(x not in {"H-A","H-B","H-C"} for x in labelled.values()):
        raise ValueError("invalid preregistered designer preferences")
    seen={}
    for r in rows:
        if r.get("experiment")!="MOOL-POLYMATH-F10-001":raise ValueError("unexpected experiment ID")
        stage=r.get("stage");arm=r.get("arm")
        if stage not in STAGES or arm not in ARMS or (stage,arm) in seen:
            raise ValueError("duplicate/unknown frozen case")
        seen[(stage,arm)]=r
        if r["hypothesis_count"]!=3 or r["f10_source_contract"]!="synthetic_unverified_only" or r["source_status"]!="project_authored_synthetic_unverified":
            raise ValueError("fabricated independent provenance")
        if r["model_world_node_count"]!=0 or r["model_world_updated"] or r["independently_verified_facts"]!=0 or r["outcome_feedback_performed"]:
            raise ValueError("synthetic case laundered into empirically verified fact")
        if r["native_dwm_capability_enabled"]!=(arm=="G2") or r["native_hypokosh_capability_enabled"]!=(arm!="G0"):
            raise ValueError("invalid live native ablation flags")
        if not r["actual_graphene_executed"]:raise ValueError("native path did not execute")
        for name in ("initial_bundle","final_bundle"):
            d=r[name]
            if any(type(d[k])!=int or d[k]<0 for k in COUNTS):
                raise ValueError("malformed path diagnostic count")
            if sum(d[k] for k in ("support_role_paths","opposition_role_paths",
                                  "noise_role_paths","unknown_role_paths"))!=d["retrieved_fiber_paths"]:
                raise ValueError("unaccounted native path roles")
            if d["opposition_representatives_eligible"]>d["opposition_eligible_paths"]:
                raise ValueError("eligible representative without opposing path")
            if d["contradiction_edges_in_paths"]>d["path_edge_count"]:
                raise ValueError("contradiction intersection exceeds actual path edges")
            if d["opposition_representatives_eligible"]>d["opposition_correlation_groups"]:
                raise ValueError("eligible representative without group")
        if not math.isfinite(r["final_opposition_score"]):
            raise ValueError("nonfinite epistemic opposition score")
        expect_edges={"baseline":(3,0),"contradiction":(4,1),"revision":(6,2)}[stage]
        if (r["input_edge_count"],r["persistent_contradiction_edges"])!=expect_edges:
            raise ValueError("raw contradiction did not persist or staging altered")
        if arm=="G2" and not r["actual_opposition_executed"]:
            raise ValueError("declared opposition not actually invoked")
        if arm!="G2" and r["actual_opposition_executed"]:
            raise ValueError("disabled ablation ran opposition")
        if r["operative_selection"] and r["selected_hypothesis"] not in {"H-A","H-B","H-C"}:
            raise ValueError("operative selection not a real hypothesis node")
        if not r["operative_selection"] and r["selected_hypothesis"]!="none":
            raise ValueError("nonoperative selection fabricated a label")
    if set(seen)!={(s,a) for s in STAGES for a in ARMS}:
        raise ValueError("incomplete actual native 3x3 ablations")
    per=[]
    for stage in STAGES:
        for arm in ARMS:
            r=seen[(stage,arm)]
            d=r["final_bundle"]
            if r["persistent_contradiction_edges"]==0:
                bottleneck="NO_RAW_CONTRADICTION_IN_STAGE"
            elif d["contradiction_edges_in_paths"]==0:
                bottleneck="RAW_CONTRADICTION_NOT_TRAVERSED_IN_PATH"
            elif d["opposition_role_paths"]==0:
                bottleneck="RETRIEVED_NO_OPPOSITION_ROLE"
            elif d["opposition_eligible_paths"]==0:
                bottleneck="OPPOSITION_INELIGIBLE"
            elif d["opposition_representatives_eligible"]==0:
                bottleneck="NO_ELIGIBLE_INDEPENDENT_CORRELATION_REPRESENTATIVE"
            elif not r["dialectical_challenge"]:
                bottleneck="ADMITTED_BUT_NO_MATERIAL_CHALLENGE"
            else:
                bottleneck="CHALLENGE_EXECUTED"
            per.append({
                "stage":stage,"arm":arm,
                "raw_contradiction_edges":r["persistent_contradiction_edges"],
                "retrieved_paths":d["retrieved_fiber_paths"],
                "path_edges_total":d["path_edge_count"],
                "persisted_contradiction_edges_traversed":d["contradiction_edges_in_paths"],
                "opposition_role_paths":d["opposition_role_paths"],
                "opposition_eligible_paths":d["opposition_eligible_paths"],
                "admissible_representative_groups":d["opposition_representatives_eligible"],
                "actual_dwm_invoked":r["actual_opposition_executed"],
                "dialectical_challenge":r["dialectical_challenge"],
                "reopen_requested":r["opposition_reopen_requested"],
                "operative_selection":r["operative_selection"],
                "selected_hypothesis":r["selected_hypothesis"],
                "synthetic_designer_label":labelled[stage],
                "matches_synthetic_designer_label":bool(r["operative_selection"] and r["selected_hypothesis"]==labelled[stage]),
                "forensic_disposition":bottleneck
            })
    summaries={}
    for arm in ARMS:
        records=[x for x in per if x["arm"]==arm]
        summaries[arm]={
            "n":len(records),
            "operative_selection_n":sum(x["operative_selection"] for x in records),
            "match_synthetic_designer_n":sum(x["matches_synthetic_designer_label"] for x in records),
            "actual_dwm_calls":sum(x["actual_dwm_invoked"] for x in records),
            "material_challenges":sum(x["dialectical_challenge"] for x in records),
            "reopen_requests":sum(x["reopen_requested"] for x in records),
            "admitted_opposition_representatives":sum(x["admissible_representative_groups"] for x in records)
        }
    return {
        "experiment":"MOOL-POLYMATH-F10-001",
        "engineering_execution":"9_REAL_NATIVE_ABLATIONS",
        "source_label_kind":"SYNTHETIC_DESIGNER_NOT_INDEPENDENTLY_ADJUDICATED",
        "independently_verified_facts":0,
        "actual_outcome_feedback":"NOT_RUN",
        "external_truth_accuracy":"NOT_MEASURABLE",
        "did_dwm_outperform_g1_on_synthetic_preference":summaries["G2"]["match_synthetic_designer_n"]>summaries["G1"]["match_synthetic_designer_n"],
        "arms":summaries,
        "receipts":per,
        "conclusions_limited_to":["Persistent graph ingestion","Path-role disposition","Native eligibility and correlation checks","Synthetic preference illustration, not scientific correctness"]
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--receipts",required=True)
    p.add_argument("--synthetic-labels",required=True)
    p.add_argument("--out",required=True)
    args=p.parse_args()
    rows=[json.loads(r) for r in Path(args.receipts).read_text().splitlines() if r.strip()]
    gold=json.loads(Path(args.synthetic_labels).read_text())
    outcome=evaluate(rows,gold)
    Path(args.out).write_text(json.dumps(outcome,sort_keys=True,indent=2)+"\n")
    for r in outcome["receipts"]:
        print("f10",r["stage"],r["arm"],"raw_conflicts",r["raw_contradiction_edges"],
              "eligible",r["admissible_representative_groups"],r["forensic_disposition"])
    print("f10_native_path_admission_forensics=PASS 9/9 external_truth=NOT_MEASURABLE")
if __name__=="__main__":main()
