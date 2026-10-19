#!/usr/bin/env python3
"""F10B real-native topology × query-mode forensic score. No empirical truth claims."""
import argparse,json,math
from pathlib import Path

def assess(records):
    if len(records)!=12:raise ValueError("missing native factorial ablations")
    keys=set()
    result=[]
    for item in records:
        if item.get("experiment")!="MOOL-POLYMATH-F10B-001":
            raise ValueError("wrong experiment version")
        topology=item["topology"];mode=item["query_mode"];arm=item["arm"]
        if topology not in ("original","corrected") or mode not in ("empirical","theoretical") or arm not in ("G0","G1","G2"):
            raise ValueError("invalid factorial arm")
        key=(topology,mode,arm)
        if key in keys:raise ValueError("duplicated factorial arm")
        keys.add(key)
        if item["stage"]!="revision" or item["hypothesis_count"]!=3 or item["input_edge_count"]!=6 or item["persistent_contradiction_edges"]!=2:
            raise ValueError("changed synthetic incident universe")
        if (item["source_status"]!="project_authored_synthetic_unverified" or
            item["f10_source_contract"]!="synthetic_unverified_only" or
            item["independently_verified_facts"]!=0 or item["outcome_feedback_performed"] or
            item["model_world_updated"] or item["model_world_node_count"]!=0):
            raise ValueError("independent empirical truth promotion in synthetic test")
        if item["actual_opposition_executed"]!=(arm=="G2") or item["native_dwm_capability_enabled"]!=(arm=="G2") or item["native_hypokosh_capability_enabled"]!=(arm!="G0"):
            raise ValueError("actual native ablation not executed as declared")
        if not item["actual_graphene_executed"]:raise ValueError("not real GrapheneDB")
        paths=item["final_bundle"]
        if paths["contradiction_edges_in_paths"]>paths["path_edge_count"]:
            raise ValueError("claimed more contradictions than traversed path edges")
        if sum(paths[k] for k in ("support_role_paths","opposition_role_paths","noise_role_paths","unknown_role_paths"))!=paths["retrieved_fiber_paths"]:
            raise ValueError("lost path-role accountability")
        if not math.isfinite(item["final_opposition_score"]):raise ValueError("invalid opposition scalar")
        if item["operative_selection"] and item["selected_hypothesis"] not in ("H-A","H-B","H-C"):
            raise ValueError("not an actual selected hypothesis")
        if not item["operative_selection"] and item["selected_hypothesis"]!="none":
            raise ValueError("invented selected hypothesis")
        if mode=="empirical" and paths["contradiction_edges_in_paths"]!=0:
            raise ValueError("hypothetical edges improperly admitted in Empirical mode")
        result.append({
            "topology":topology,"mode":mode,"arm":arm,
            "persisted_contradiction_edges":2,
            "retrieved_fiber_paths":paths["retrieved_fiber_paths"],
            "traversed_path_edges":paths["path_edge_count"],
            "traversed_contradictory_edges":paths["contradiction_edges_in_paths"],
            "opposition_role_paths":paths["opposition_role_paths"],
            "opposition_eligible_paths":paths["opposition_eligible_paths"],
            "independent_eligible_representatives":paths["opposition_representatives_eligible"],
            "native_dwm_oppose_executed":item["actual_opposition_executed"],
            "challenge":item["dialectical_challenge"],
            "reopen":item["opposition_reopen_requested"],
            "status":item["status"],"selected_hypothesis":item["selected_hypothesis"],
            "external_truth":"NOT_EVALUATED"})
    if len(keys)!=12:raise ValueError("incomplete 2x2x3 actual factorial")
    summary=[]
    for topology in ("original","corrected"):
        for mode in ("empirical","theoretical"):
            arms=[x for x in result if x["topology"]==topology and x["mode"]==mode]
            summary.append({
                "topology":topology,"query_mode":mode,
                "actual_runtime_calls":3,
                "maximum_traversed_edge_count":max(x["traversed_path_edges"] for x in arms),
                "maximum_traversed_contradiction_count":max(x["traversed_contradictory_edges"] for x in arms),
                "maximum_opposition_role_paths":max(x["opposition_role_paths"] for x in arms),
                "maximum_opposition_eligible_paths":max(x["opposition_eligible_paths"] for x in arms),
                "dwm_challenges":sum(x["challenge"] for x in arms),
                "dwm_reopen_requests":sum(x["reopen"] for x in arms)
            })
    return {
        "experiment":"MOOL-POLYMATH-F10B-001",
        "source_authenticity":"ALL_PROJECT_AUTHORED_UNVERIFIED_HYPOTHETICAL",
        "actual_executions":12,"external_truth_accuracy":"NOT_EVALUATED",
        "modelworld_writes":0,"new_independently_verified_facts":0,
        "interpretation":"MEASUREMENT_DIAGNOSTIC_ONLY",
        "factorial_summary":summary,
        "raw_per_arm":result,
        "notes":[
            "Original topology stops at zero-hop root candidates",
            "Empirical query mode intentionally forbids Hypothetical edges",
            "Theoretical mode can expose hypothetical paths without authenticating them",
            "Opposition role alone is not eligible material evidence",
            "No independent outcome feedback or causal accuracy was measured"]
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--receipts",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    raw=[json.loads(x) for x in Path(a.receipts).read_text().splitlines() if x.strip()]
    scores=assess(raw)
    Path(a.out).write_text(json.dumps(scores,sort_keys=True,indent=2)+"\n")
    for x in scores["factorial_summary"]:
        print("f10b",x["topology"],x["query_mode"],
              "max_path_edges",x["maximum_traversed_edge_count"],
              "contradiction_edges_traversed",x["maximum_traversed_contradiction_count"],
              "material_challenges",x["dwm_challenges"])
    print("f10b_native_factorial_forensics=PASS 12/12 verified_new_facts=0")
if __name__=="__main__":main()
