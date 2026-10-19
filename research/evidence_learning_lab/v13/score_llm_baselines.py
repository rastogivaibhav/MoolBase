#!/usr/bin/env python3
"""Score completed actual Qwen baseline completions against sealed SQL interventions."""
import argparse,json,math,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from score_event_lab import load_oracles,FAULT_TO_H
from run_llm_baselines import selected,load_public
def grade(preds,oracles,public):
 if preds.get("experiment")!="MOOL-EVIDENCE-LEARNING-V13B-001" or not preds.get("real_model_executed"):
  raise ValueError("not actual prescribed LLM result")
 rows=preds["records"]
 if len(rows)!=24:raise ValueError("must execute 24 genuine completions")
 log=load_public(public)
 expected={(c,t,a) for c in oracles for t in ("T0","T1") for a in ("L0","L1")}
 used=set();samples=[]
 for p in rows:
  key=(p["case_id"],p["cutoff"],p["arm"])
  if key not in expected or key in used:raise ValueError("missing or duplicate LLM case")
  used.add(key)
  event_ids=[r["event_id"] for r in selected(log,p["case_id"],p["cutoff"])]
  if p["events_used"]!=event_ids:
   raise ValueError("model did not receive exact time cutoff event set")
  prompt=p["question"]
  if any(x in prompt for x in ('"injected_fault"', '"outcome_repaired"', '"stage":"T2"', 'stage=T2')):
   raise ValueError("controller outcome or future event visible to inference")
  if p["parsed_choice"] not in ("none","H-A","H-B","H-C"):
   raise ValueError("invalid model label")
  if not math.isfinite(p["seconds"]) or p["seconds"]<0:raise ValueError("missing real model latency")
  truth=FAULT_TO_H[oracles[p["case_id"]]["injected_fault"]]
  valid=p["parse_error"] is None
  samples.append({"case_id":p["case_id"],"cutoff":p["cutoff"],"arm":p["arm"],
   "correct_internal_label":valid and p["parsed_choice"]==truth,
   "valid_output":valid,"proposed":valid and p["parsed_choice"]!="none",
   "wrong_proposal":valid and p["parsed_choice"] not in ("none",truth),
   "withheld":valid and p["parsed_choice"]=="none",
   "reason":p["reason"],"answer":p["parsed_choice"],"gold_internal":truth,
   "runtime_seconds":p["seconds"]})
 if used!=expected:raise ValueError("incomplete LLM benchmarking matrix")
 scores={}
 for arm in ("L0","L1"):
  rs=[r for r in samples if r["arm"]==arm]
  timepoint={t:[r for r in rs if r["cutoff"]==t] for t in ("T0","T1")}
  wrong_corrected=0;new_proposal=0
  for cid in oracles:
   t0=next(r for r in rs if r["case_id"]==cid and r["cutoff"]=="T0")
   t1=next(r for r in rs if r["case_id"]==cid and r["cutoff"]=="T1")
   wrong_corrected+=t0["wrong_proposal"] and t1["correct_internal_label"]
   new_proposal+=t0["withheld"] and t1["proposed"]
  scores[arm]={
    "n":len(rs),"t0_withheld":sum(x["withheld"] for x in timepoint["T0"]),
    "t1_correct_internal_label":sum(x["correct_internal_label"] for x in timepoint["T1"]),
    "t1_wrong_proposal":sum(x["wrong_proposal"] for x in timepoint["T1"]),
    "t1_useful_proposals":sum(x["proposed"] for x in timepoint["T1"]),
    "parse_failures":sum(not x["valid_output"] for x in rs),
    "wrong_prior_proposals_corrected":wrong_corrected,
    "new_proposals_after_diagnostics":new_proposal,
    "average_response_seconds":sum(x["runtime_seconds"] for x in rs)/len(rs)}
 return {"experiment":"MOOL-EVIDENCE-LEARNING-V13B-001",
 "executed_real_model_calls":24,"model_sha256":preds["model_sha256"],
 "backend_sha":preds["git_sha"],
 "independent_real_world_outcomes":False,
 "actual_erpnext_connection":False,
 "model_weight_learning":"NOT_RUN",
 "source_authentication":False,
 "arms":scores,"case_receipts":samples,
 "limitations":["6 public project-generated fault cases, not expert-blind benchmark",
 "Both arms use identical event records and same pinned model but formatted differently",
 "Exact-ID chronological retrieval is not semantic RAG",
 "Model prompt/token lengths may vary between formatting arms",
 "Comparative outcome is internal intervention concordance, not proven external causality"]}
def main():
 p=argparse.ArgumentParser()
 for k in ("predictions","oracle-dir","public","out"):p.add_argument("--"+k,required=True)
 a=p.parse_args()
 d=json.loads(Path(a.predictions).read_text())
 r=grade(d,load_oracles(a.oracle_dir),a.public)
 Path(a.out).write_text(json.dumps(r,sort_keys=True,indent=2)+"\n")
 for k,v in r["arms"].items():
  print(f"v13b {k} valid_fault_labels_T1={v['t1_correct_internal_label']}/6 "
        f"wrong_T1={v['t1_wrong_proposal']} t0_withheld={v['t0_withheld']}/6 "
        f"malformed={v['parse_failures']} correction={v['wrong_prior_proposals_corrected']}")
 print("v13b_real_7b_scoring=PASS independent_sci_accuracy=NOT_MEASURED")
if __name__=="__main__":main()
