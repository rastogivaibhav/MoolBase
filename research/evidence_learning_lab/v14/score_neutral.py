#!/usr/bin/env python3
"""V14 post-run scorer. Internal six incident labels are NOT independent truth."""
import argparse,hashlib,json,math
from pathlib import Path

EXPECTED={"LAB-001":"H-A","LAB-002":"H-A",
          "LAB-003":"H-B","LAB-004":"H-B",
          "LAB-005":"H-C","LAB-006":"H-C"}
TIMES=("T0","T1")
ARMS=("G1","G2")

def score(neutral,v13,baseline,oracles,raw_receipts):
    if len(neutral)!=24:raise ValueError("neutral native experiment is incomplete")
    if len(v13)!=24:raise ValueError("frozen mapped v13 actual native comparison missing")
    ids=set()
    mat={}
    for x in neutral:
        key=(x.get("case_id"),x.get("cutoff"),x.get("arm"))
        if (key in ids or key[0] not in EXPECTED or key[1] not in TIMES
              or key[2] not in ARMS or x.get("experiment")!="MOOL-EVIDENCE-LEARNING-V14-001"):
            raise ValueError("invalid or duplicate neutral native evaluation")
        ids.add(key);mat[key]=x
        expected_fields=2 if key[1]=="T0" else 9
        if (x["raw_numeric_fields"]!=expected_fields or
            x["native_quarantined_edges"]!=expected_fields or
            x["hypothesis_labeled_edges"]!=0):
            raise ValueError("causal label leakage or missing actual raw inputs")
        if (x["source_authenticated"] or x["scientific_truth_verified"] or
            x["model_world_updated"] or x["actual_dwm_opposition"]!=(key[2]=="G2")):
            raise ValueError("fabricated authority, feedback or DWM execution")
        if x["selected_hypothesis"] not in ("none","H-A","H-B","H-C"):
            raise ValueError("fabricated model hypothesis")
        if not math.isfinite(x["time_ms"]) or x["time_ms"]<0:
            raise ValueError("missing native runtime timing")
    required={(cid,at,arm) for cid in EXPECTED for at in TIMES for arm in ARMS}
    if ids!=required:raise ValueError("incomplete 6x2x2 neutral matrix")
    old={}
    for x in v13:
        k=(x.get("case_id"),x.get("cutoff"),x.get("arm"))
        if k in old or k not in required or x.get("experiment")!="MOOL-EVIDENCE-LEARNING-V13-001":
            raise ValueError("invalid previous native result")
        old[k]=x
    if set(old)!=required:raise ValueError("incomplete prior mapped native matrix")
    labels={x["case_id"]:x for x in oracles}
    if set(labels)!=set(EXPECTED):raise ValueError("misaligned fault controller")
    fault_to_id={"pending_receipt":"H-A","stale_snapshot":"H-B","reorder_rule":"H-C"}
    for cid,item in labels.items():
        if (item.get("independent_external_adjudication") is not False or
            item.get("outcome_repaired") is not True or
            fault_to_id.get(item.get("injected_fault"))!=EXPECTED[cid]):
            raise ValueError("unreviewed outcome oracle or source labels")
    if len(raw_receipts)!=12 or any(r.get("oracle_accessed") or r.get("future_T2_used") or r.get("cause_labels_emitted") for r in raw_receipts):
        raise ValueError("neutral adapter did not preserve oracle and time isolation")
    summary={}
    for arm in ARMS:
        old_t1=[old[cid,"T1",arm]["selected_hypothesis"] for cid in EXPECTED]
        new_t1=[mat[cid,"T1",arm]["selected_hypothesis"] for cid in EXPECTED]
        old_n=sum(c==EXPECTED[cid] for cid,c in zip(EXPECTED,old_t1))
        new_n=sum(c==EXPECTED[cid] for cid,c in zip(EXPECTED,new_t1))
        before=[mat[cid,"T0",arm]["selected_hypothesis"] for cid in EXPECTED]
        summary[arm]={
          "observed_T0_proposals":sum(c!="none" for c in before),
          "observed_T1_proposals":sum(c!="none" for c in new_t1),
          "T1_matches_internal_controller_only":new_n,
          "T1_wrong_proposals":sum(c!="none" and c!=EXPECTED[cid] for cid,c in zip(EXPECTED,new_t1)),
          "V13_cause_labeled_T1_matches_internal_controller":old_n,
          "V13_mapped_minus_V14_neutral_matches":old_n-new_n,
          "new_proposals_after_evidence":sum(b=="none" and a!="none" for b,a in zip(before,new_t1)),
          "real_wrong_proposal_corrected":sum(
             b!="none" and b!=EXPECTED[cid] and a==EXPECTED[cid]
             for cid,b,a in zip(EXPECTED,before,new_t1)),
          "actual_dwm_challenges":sum(mat[cid,at,arm]["dialectical_challenge"] for cid in EXPECTED for at in TIMES),
          "mean_native_time_ms":sum(mat[cid,at,arm]["time_ms"] for cid in EXPECTED for at in TIMES)/12
        }
    return {"experiment":"MOOL-EVIDENCE-LEARNING-V14-001",
      "same_sql_data_source":"V13_SOURCE_SYSTEM_SIMULATOR_REPLAY",
      "native_calls":24,"neutral_field_observations":66,
      "oracle_used_during_inference":False,
      "external_empirical_truth_accuracy":"NOT_ESTABLISHED",
      "authenticated_enterprise_source":False,
      "real_world_learning":"NOT_RUN",
      "independent_data":"NO",
      "arms":summary,
      "limitations":[
        "This removes the cause-labeling adapter and also alters graph topology, so it cannot isolate a unique intelligence effect.",
        "Both V13 and V14 use internally generated, public fault-control code; agreement with internal intervention is not blinded causal accuracy.",
        "The source checks establish a code/data boundary, not independent issuer identity or claim verification.",
        "Neither model weights nor persistent post-outcome hypothesis preferences were updated.",
        "Direct versus chronological-retrieval LLM baselines are separate registered V13B experiments and not fused into a common runtime."
      ]}
def main():
    p=argparse.ArgumentParser()
    for f in ("neutral","v13","baseline","oracle-dir","adapter-receipts","out"):
        p.add_argument("--"+f,required=True)
    a=p.parse_args()
    def jsonl(n):return [json.loads(x) for x in Path(n).read_text().splitlines() if x.strip()]
    n=jsonl(a.neutral);v=jsonl(a.v13)
    base=json.loads(Path(a.baseline).read_text())
    oracle=[json.loads(x.read_text()) for x in sorted(Path(a.oracle_dir).glob("*.oracle.json"))]
    receipts=json.loads(Path(a.adapter_receipts).read_text())
    result=score(n,v,base,oracle,receipts)
    Path(a.out).write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
    for arm,s in result["arms"].items():
        print(f"V14 {arm}: baseline mapped={s['V13_cause_labeled_T1_matches_internal_controller']}/6, neutral={s['T1_matches_internal_controller_only']}/6, proposals={s['observed_T1_proposals']}/6, challenges={s['actual_dwm_challenges']}")
    print("V14 neutral ablation scored; independent empirical accuracy=NOT_ESTABLISHED")
if __name__=="__main__":main()
