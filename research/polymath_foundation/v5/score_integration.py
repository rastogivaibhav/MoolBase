#!/usr/bin/env python3
"""Grade only actual v5 inference receipts; never score a simulated model arm."""
import argparse
import json
from pathlib import Path
import statistics
import sys

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v4"))
from challenge import cards_from,sha
from scorer import grade
from grade_pilot import metrics
from run_integration import selected,ARMS,V5
from native_manifest import read_native


def evaluate(cards,raw,native):
    full,tasks=selected(cards)
    ids=[task["task_id"] for task in tasks]
    if raw.get("experiment")!=V5 or raw.get("status")!="ACTUAL_MODEL_RESPONSES":
        raise ValueError("not actual v5 model output")
    if raw.get("full_challenge_sha256")!=sha(full) or raw.get("source_card_sha256")!=sha(cards):
        raise ValueError("wrong frozen question/card hash")
    if raw.get("selected_task_ids")!=ids:
        raise ValueError("not prespecified task sample")
    if raw.get("native_context")!=native or raw.get("verified_new_facts")!=0:
        raise ValueError("native source/epistemic evidence mismatch")
    if raw.get("hypokosh_dwm_runtime_executed") is not True or raw.get("moolbase_native_db_executed") is not True:
        raise ValueError("native model world integration not executed")
    if len(raw.get("model_sha256",""))!=64:
        raise ValueError("real model file digest required")
    result={
        "experiment":V5,"status":"COMPLETE_DESCRIPTIVE_PUBLIC_PILOT",
        "research_claim":"NO_INDEPENDENT_POLYMATH_COMPETENCE_ESTABLISHED",
        "full_challenge_sha256":sha(full),
        "model_file_sha256":raw["model_sha256"],
        "server_commit":raw["server_commit"],
        "github_commit":raw["github_commit"],
        "sample_count":12,
        "sample_valid_n":0,
        "sample_boundary_n":0,
        "executed_actual_native_moolbase":True,
        "executed_actual_hypokosh_dwm":True,
        "native_new_verified_facts":0,
        "arms":{},
        "limitations":[
            "public deterministic pedagogical benchmark, no blinded independent evaluator",
            "short 12-task non-representative developmental pilot",
            "native retrieval with exactly one card per domain is intentionally equivalent to direct-card prompting",
            "DWM emits epistemic status/trace rather than numeric predictions",
            "fixed decoding tokens but different input lengths, not compute matched",
            "no source-authenticated scientific observation or cross-domain discovery"
        ]
    }
    per={}
    for arm in ARMS:
        x=raw.get("arms",{}).get(arm)
        if not x or x.get("model_executed") is not True or len(x.get("records",[]))!=12:
            raise ValueError("actual arm missing: "+arm)
        rows=x["records"]
        if [r.get("task_id") for r in rows]!=ids or any(
            not isinstance(r.get("raw_model_output"),str) or
            not isinstance(r.get("prompt"),str) or
            "adapted_response" not in r for r in rows):
            raise ValueError("missing/forged raw response")
        parsed={r["task_id"]:r["adapted_response"] for r in rows}
        # The original scorer requires the full frozen benchmark; nonsampled
        # IDs are explicitly false-labeled and excluded after grading.
        answers=[]
        for task in full["tasks"]:
            if task["task_id"] in parsed:
                answers.append(parsed[task["task_id"]])
            else:
                answers.append({
                    "task_id":task["task_id"],"answer_type":"abstain",
                    "reason":"Not included in frozen twelve-task F5 sample.",
                    "evidence_card_id":"NOT_SAMPLED",
                    "assumptions_acknowledged":False,"confidence":0
                })
        receipt=grade(full,cards,{
            "experiment":"MOOL-POLYMATH-F3-001",
            "participant":"real-Qwen-1.5B-"+arm,
            "challenge_sha256":sha(full),
            "model_executed":True,
            "responses":answers
        })
        subset=[r for r in receipt["case_receipts_without_gold"]
                if r["task_id"] in parsed]
        assert len(subset)==12
        for r in rows:
            if r.get("parser_error") and next(s for s in subset
                                             if s["task_id"]==r["task_id"])["correct"]:
                raise ValueError("malformed response scored correct")
        by_domain={
            d:metrics([r for r in subset if r["domain"]==d])
            for d in ("networks","biology","supply_chain")
        }
        per[arm]={r["task_id"]:r for r in subset}
        result["arms"][arm]={
            "score":metrics(subset),"per_domain":by_domain,
            "parser_failure_count":sum(bool(r.get("parser_error")) for r in rows),
            "total_output_tokens_reported":sum(
                (r.get("usage") or {}).get("completion_tokens",0) for r in rows),
            "total_input_tokens_reported":sum(
                (r.get("usage") or {}).get("prompt_tokens",0) for r in rows),
            "mean_response_seconds":statistics.mean(r["elapsed_seconds"] for r in rows),
            "task_receipts":subset
        }
    origin=per["A0"]
    for arm in ARMS[1:]:
        rows=per[arm]
        result["arms"][arm]["delta_accuracy_vs_A0"]=(
            result["arms"][arm]["score"]["total_accuracy"]-
            result["arms"]["A0"]["score"]["total_accuracy"])
        result["arms"][arm]["paired_wins_only_this_arm"]=sum(
            rows[t]["correct"] and not origin[t]["correct"] for t in ids)
        result["arms"][arm]["paired_losses_only_this_arm"]=sum(
            not rows[t]["correct"] and origin[t]["correct"] for t in ids)
    sample0=result["arms"]["A0"]["score"]
    result["sample_valid_n"]=sample0["valid_n"]
    result["sample_boundary_n"]=sample0["boundary_n"]
    return result


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--native",required=True)
    p.add_argument("--inference",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    cards=cards_from(a.cards)
    native=read_native(cards,a.native)
    raw=json.loads(Path(a.inference).read_text(encoding="utf-8"))
    scored=evaluate(cards,raw,native)
    Path(a.out).write_text(json.dumps(scored,sort_keys=True,indent=2)+"\n")
    for arm in ARMS:
        s=scored["arms"][arm]["score"]
        print(f"F5 {arm} correct={s['correct_n']}/12 "
              f"application={s['valid_application_accuracy']:.3f} "
              f"boundary={s['boundary_abstention_rate']} "
              f"parser_errors={scored['arms'][arm]['parser_failure_count']}")
    print("f5_scored_real_native_and_model=PASS "
          "new_verified_facts=0 polymath_claim=NOT_ESTABLISHED")


if __name__=="__main__":
    main()
