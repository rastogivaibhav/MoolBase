#!/usr/bin/env python3
"""Validate actual-native Gate2 receipts without inventing correct cause labels."""
import argparse,json,math,hashlib
from pathlib import Path

def validate(lines):
    if len(lines)!=9:
        raise ValueError("Gate2 not complete: expected 9 real native reasoner receipts")
    bykey={}
    for item in lines:
        if item.get("experiment")!="MOOL-POLYMATH-F9-G2-001":
            raise ValueError("wrong experiment source")
        stage=item["stage"];arm=item["arm"]
        if stage not in ("baseline","contradiction","revision") or arm not in ("G0","G1","G2"):
            raise ValueError("unregistered snapshot/ablation")
        key=(stage,arm)
        if key in bykey:raise ValueError("duplicate native condition")
        bykey[key]=item
        if item["hypothesis_count"]!=3 or item["source_status"]!="project_authored_synthetic_unverified":
            raise ValueError("synthetic source or hypothesis count corrupted")
        if (item["model_world_updated"] or item["model_world_node_count"]!=0 or
            item["independently_verified_facts"]!=0 or item["outcome_feedback_performed"]):
            raise ValueError("false promotion or fabricated empirical outcome")
        if item["native_hypokosh_capability_enabled"]!=(arm!="G0") or item["native_dwm_capability_enabled"]!=(arm=="G2"):
            raise ValueError("real runtime ablation flags mismatch")
        if not item["actual_graphene_executed"]:
            raise ValueError("real native evidence expansion did not run")
        for key2 in ("confidence","initial_opposition_score","final_opposition_score"):
            if not math.isfinite(item[key2]):raise ValueError("nonfinite runtime score")
    if set(bykey)!={(stage,arm) for stage in ("baseline","contradiction","revision") for arm in ("G0","G1","G2")}:
        raise ValueError("missing one or more frozen native runs")
    report={
        "experiment":"MOOL-POLYMATH-F9-G2-001",
        "complete_actual_native_runtime_calls":9,
        "hypotheses_per_snapshot":3,
        "competing_snapshots":3,
        "actual_opposition_executions":sum(v["actual_opposition_executed"] for v in lines),
        "g2_actual_opposition_executions":sum(v["actual_opposition_executed"] for v in lines if v["arm"]=="G2"),
        "g2_actual_dialectical_challenges":sum(v["dialectical_challenge"] for v in lines if v["arm"]=="G2"),
        "g2_requested_reopens":sum(v["opposition_reopen_requested"] for v in lines if v["arm"]=="G2"),
        "g2_changed_primary_node_across_snapshots":len({v["primary_node"] for v in lines if v["arm"]=="G2"})>1,
        "g2_changed_epistemic_status_across_snapshots":len({v["status"] for v in lines if v["arm"]=="G2"})>1,
        "independently_verified_facts":0,
        "external_feedback_gate":"NOT_RUN",
        "science_correctness_gate":"NOT_ASSESSED",
        "independent_blind":"NO",
        "improved_reasoning_claim":"NOT_ESTABLISHED",
        "native_statuses":{s:{a:bykey[(s,a)]["status"] for a in ("G0","G1","G2")}
          for s in ("baseline","contradiction","revision")},
        "limitations":["Three constructed hypotheses and snapshots, no authenticated empirical outcome",
          "Native DWM traces indicate control-flow execution, not epistemic superiority",
          "Comparing status changes alone does not measure truth or causal explanatory accuracy"]
    }
    return report

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--inference",required=True);p.add_argument("--out",required=True)
    a=p.parse_args()
    lines=[json.loads(row) for row in Path(a.inference).read_text().splitlines() if row.strip()]
    x=validate(lines)
    Path(a.out).write_text(json.dumps(x,sort_keys=True,indent=2)+"\n")
    print("f9_gate2_native_trace_validation=PASS 9/9 actual reasoner calls "
          f"G2_opposition_executions={x['g2_actual_opposition_executions']} "
          "external_truth=NOT_ASSESSED")
if __name__=="__main__":
    main()
