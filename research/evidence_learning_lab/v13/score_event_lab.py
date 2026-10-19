#!/usr/bin/env python3
"""Evaluate executed SQL fault-outcomes after native inference is sealed."""
import argparse,json,math
from pathlib import Path
FAULT_TO_H={"pending_receipt":"H-A","stale_snapshot":"H-B","reorder_rule":"H-C"}
def load_oracles(folder):
    docs=[json.loads(p.read_text()) for p in sorted(Path(folder).glob("*.oracle.json"))]
    if len(docs)!=6:raise ValueError("missing actual SQL intervention outcomes")
    index={}
    for d in docs:
        case_id=d["case_id"]
        if case_id in index or d["injected_fault"] not in FAULT_TO_H:
            raise ValueError("invalid actual intervention ID")
        if d["independent_external_adjudication"] is not False or d["outcome_repaired"] is not True:
            raise ValueError("fabricated external adjudication or failed repair")
        before=d["t1_before"];after=d["t2_after"]
        if after["erp"]["pending_receipts"]!=0 or after["erp"]["posted"]!=after["wms"]["actual"] or after["wms"]["snapshot"]!=after["wms"]["actual"] or after["erp"]["reorder_min"]<10:
            raise ValueError("SQL repaired state not actually resolved")
        if before==after:raise ValueError("no SQL state change under repair")
        index[case_id]=d
    return index

def evaluate(native,deterministic,oracles):
    true_ids=set(oracles)
    expected={(case_id,t,a) for case_id in true_ids for t in ("T0","T1") for a in ("G1","G2")}
    native_ids=set()
    scores={}
    for row in native:
        cid=row["case_id"];cutoff=row["cutoff"];arm=row["arm"]
        key=(cid,cutoff,arm)
        if key not in expected or key in native_ids:raise ValueError("missing or duplicated native evaluation")
        native_ids.add(key)
        if (row.get("experiment")!="MOOL-EVIDENCE-LEARNING-V13-001" or
            row["source_quarantined"] is not True or row["source_authenticated"] is not False or
            row["scientific_truth_verified"] is not False or row["model_world_updated"] is not False or
            row["actual_native_opposition"]!=(arm=="G2")):
            raise ValueError("native receipt attempted false scientific promotion")
        if (row["input_signal_count"]!=(3 if cutoff=="T0" else 6) or
            row["persisted_graph_edges"]!=(4 if cutoff=="T0" else 7)):
            raise ValueError("graph evidence not consistent with event cutoff")
        if row["selected_hypothesis"] not in ("none","H-A","H-B","H-C"):
            raise ValueError("unexpected selected hypothesis")
        if not math.isfinite(row["elapsed_ms"]) or row["elapsed_ms"]<0:
            raise ValueError("invalid latency")
    if native_ids!=expected:raise ValueError("incomplete 24 real native evaluations")
    base_expected={(case_id,t) for case_id in true_ids for t in ("T0","T1")}
    baseline={}
    for row in deterministic:
        key=(row["case_id"],row["cutoff"])
        if key not in base_expected or key in baseline:raise ValueError("invalid deterministic rule baseline")
        if row["source"]!="PROJECT_AUTHORED_DETERMINISTIC_RULE_BASELINE":
            raise ValueError("baseline claimed learning or independent evidence")
        if row["prediction"] not in ("none","H-A","H-B","H-C"):raise ValueError("unsupported baseline")
        baseline[key]=row["prediction"]
    if set(baseline)!=base_expected:raise ValueError("incomplete deterministic baseline")
    result=[]
    for row in native:
        truth=FAULT_TO_H[oracles[row["case_id"]]["injected_fault"]]
        result.append({
          "case_id":row["case_id"],"cutoff":row["cutoff"],"arm":row["arm"],
          "selected":row["selected_hypothesis"],"internal_controller_fault":truth,
          "matched_injected_label":row["selected_hypothesis"]==truth,
          "proposed":row["selected_hypothesis"]!="none",
          "challenge":row["challenge"],"reopen":row["reopen"],
          "epistemic_status":row["status"],"elapsed_ms":row["elapsed_ms"]})
    summary={}
    for arm in ("R0","G1","G2"):
        arr=[]
        for case_id in sorted(oracles):
            for cutoff in ("T0","T1"):
                cause=FAULT_TO_H[oracles[case_id]["injected_fault"]]
                if arm=="R0":
                    selected=baseline[case_id,cutoff]
                    arr.append({"case_id":case_id,"cutoff":cutoff,"proposed":selected!="none",
                      "matched":selected==cause,"selected":selected,"challenge":False,"elapsed_ms":None})
                else:
                    r=next(x for x in result if x["case_id"]==case_id and x["cutoff"]==cutoff and x["arm"]==arm)
                    arr.append({"case_id":case_id,"cutoff":cutoff,"proposed":r["proposed"],
                      "matched":r["matched_injected_label"],"selected":r["selected"],
                      "challenge":r["challenge"],"elapsed_ms":r["elapsed_ms"]})
        corrections=0
        new_predictions=0
        for case_id in sorted(oracles):
            old=next(x for x in arr if x["case_id"]==case_id and x["cutoff"]=="T0")
            new=next(x for x in arr if x["case_id"]==case_id and x["cutoff"]=="T1")
            new_predictions+=not old["proposed"] and new["proposed"]
            corrections+=old["proposed"] and not old["matched"] and new["matched"]
        summary[arm]={
            "n":len(arr),"proposals":sum(x["proposed"] for x in arr),
            "matching_internal_injected_cause":sum(x["matched"] for x in arr),
            "t1_matching_injected_cause":sum(x["matched"] and x["cutoff"]=="T1" for x in arr),
            "new_proposals_after_diagnostics":new_predictions,
            "wrong_prior_cause_corrected":corrections,
            "challenge_events":sum(x["challenge"] for x in arr),
            "mean_ms":(sum(x["elapsed_ms"] for x in arr)/len(arr)) if arm!="R0" else None
        }
    return {"experiment":"MOOL-EVIDENCE-LEARNING-V13-001",
      "executed_sql_incidents":6,"native_G1_G2_calls":24,
      "handwritten_baseline_calls":12,
      "actual_erpnext_used":False,"genuine_enterprise_sources":False,
      "authenticated_external_source":False,"independent_expert_cases":False,
      "observed_repair_transactions":6,
      "llm_only_baseline":"NOT_RUN","llm_rag_baseline":"NOT_RUN",
      "future_case_learning":"NOT_RUN",
      "empirical_scientific_accuracy":"NOT_ESTABLISHED",
      "matched_native_budget_except_dwm_overhead":True,
      "results":summary,"native_receipts":result,
      "limitations":["Project-authored SQL simulator and deterministic event mapper",
        "The oracle is separate at runtime but source-visible, not independently blind",
        "Rule baseline uses the same derived event logic as native graph mapping",
        "All graph claims remain Hypothetical in Theoretical mode",
        "No external event signature, independent adjudication or model learning"]}
def main():
    p=argparse.ArgumentParser()
    for name in ("native","baseline","oracle-dir","out"):
        p.add_argument("--"+name,required=True)
    a=p.parse_args()
    raw=[json.loads(x) for x in Path(a.native).read_text().splitlines() if x.strip()]
    base=json.loads(Path(a.baseline).read_text())
    result=evaluate(raw,base,load_oracles(a.oracle_dir))
    Path(a.out).write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
    for arm,s in result["results"].items():
        print(f"v13 {arm} T1_matching_fixture={s['t1_matching_injected_cause']}/6 "
              f"new_proposals={s['new_proposals_after_diagnostics']} "
              f"wrong_prior_corrections={s['wrong_prior_cause_corrected']} "
              f"challenge_events={s['challenge_events']}")
    print("v13_source_transaction_adjudication=PASS scientific_accuracy=NOT_ESTABLISHED")
if __name__=="__main__":main()
