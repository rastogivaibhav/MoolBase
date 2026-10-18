#!/usr/bin/env python3
"""Actual 1.5B Qwen LLM on v5 four fixed arms with real native DB/DWM receipts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v4"))
from challenge import cards_from,generate,sha
from run_actual_model import build_prompt,call_server,adapt,sha_file,SYSTEM
from native_manifest import read_native

V5="MOOL-POLYMATH-F5-001"
ARMS=("A0","A1","A2","A3")


def selected(cards):
    full=generate(cards)
    targets=[]
    for domain in ("networks","biology","supply_chain"):
        subset=[t for t in full["tasks"] if t["domain"]==domain]
        if len(subset)!=24:
            raise ValueError("modified frozen v3 domain split")
        targets.extend(subset[8:12])
    if len(targets)!=12:
        raise ValueError("v5 freeze expected 12")
    return full,targets


def native_card(row):
    return {"id":row["card_id"],"domain":row["domain"],
            "title":row["title"],"description":row["description"],
            "assumptions":row["assumptions"].split("; "),
            "must_abstain_when":row["boundaries"].split("; ")}


def prompt_for(arm,task,cards,native):
    base=cards[task["card_id"]]
    if arm=="A0":
        return build_prompt(task,base,"A0")
    if arm=="A1":
        return build_prompt(task,base,"A1")
    n=native[task["domain"]]
    retrieved=native_card(n)
    text=build_prompt(task,retrieved,"A1")
    # The underlying prompt is intentionally identical for A1 and A2,
    # isolating content storage/retrieval rather than giving ontology credit
    # for a different text.
    direct=build_prompt(task,base,"A1")
    if text!=direct:
        raise ValueError("A1 and A2 prompts differ; cannot attribute effects")
    if arm=="A2":
        return text
    if arm=="A3":
        return (
            "NATIVE MOOLBASE / HYPOKOSH-DWM EPISTEMIC RECEIPT:\n"
            f"Declared source: project-authored pedagogical; not independently verified.\n"
            f"Native actual runtime status: {n['dwm_status']}.\n"
            f"Native candidate node ID: {n['dwm_primary_node']} (not a proof).\n"
            f"Referenced graph edges: {n['dwm_evidence_count']}; all are hypothetical and unverified.\n"
            "Treat this as uncertainty metadata only; do not invent evidence, "
            "change the given mathematical equations or infer a numeric answer "
            "from the engine status. Follow assumptions and abstention criteria.\n\n"
            + text)
    raise ValueError("unknown arm")


def run(args):
    cards=cards_from(args.cards)
    canonical={c["id"]:c for c in cards["cards"]}
    retrieved=read_native(cards,args.native)
    full,tasks=selected(cards)
    meta={
        "experiment":V5,
        "status":"ACTUAL_MODEL_RESPONSES",
        "model_name":args.model,
        "model_sha256":sha_file(args.model_file),
        "server_commit":args.server_commit,
        "github_commit":os.environ.get("GITHUB_SHA","LOCAL_UNVERIFIED"),
        "full_challenge_sha256":sha(full),
        "source_card_sha256":sha(cards),
        "native_receipt_sha256":sha_file(args.native),
        "selected_task_ids":[t["task_id"] for t in tasks],
        "sample_selection":"domain_local_index_8_through_11",
        "inference":{"temperature":0,"seed":20261018,"max_tokens":160,"context":4096},
        "native_context":retrieved,
        "hypokosh_dwm_runtime_executed":True,
        "moolbase_native_db_executed":True,
        "verified_new_facts":0,
        "arms":{}
    }
    path=Path(args.output)
    path.parent.mkdir(parents=True,exist_ok=True)
    for arm in ARMS:
        records=[]
        for i,task in enumerate(tasks):
            user=prompt_for(arm,task,canonical,retrieved)
            started=time.monotonic()
            reply=call_server(
                args.url,args.model,
                [{"role":"system","content":SYSTEM},
                 {"role":"user","content":user}],
                args.timeout)
            duration=time.monotonic()-started
            options=reply.get("choices",[])
            if not options or not isinstance(options[0].get("message",{}).get("content"),str):
                raise RuntimeError("native model inference returned no text")
            raw=options[0]["message"]["content"]
            parsed,error=adapt(raw,task)
            records.append({
                "task_id":task["task_id"],"arm":arm,
                "prompt":user,"system_prompt":SYSTEM,
                "raw_model_output":raw,"adapted_response":parsed,
                "parser_error":error,"elapsed_seconds":duration,
                "usage":reply.get("usage",{}),
                "model_reported_by_server":reply.get("model"),
                "finish_reason":options[0].get("finish_reason")
            })
            print(f"v5 {arm} {i+1:02d}/12 parse={'FAIL' if error else 'OK'} "
                  f"seconds={duration:.2f}",flush=True)
        meta["arms"][arm]={
            "model_executed":True,"records":records,
            "parser_failure_count":sum(bool(x["parser_error"]) for x in records)
        }
        path.write_text(json.dumps(meta,indent=2,sort_keys=True)+"\n")
    print("f5_actual_native_and_model_arms=PASS A0,A1,A2,A3")
    return meta


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--native",required=True)
    p.add_argument("--model-file",required=True)
    p.add_argument("--server-commit",required=True)
    p.add_argument("--model",default="qwen2.5-1.5b-instruct")
    p.add_argument("--url",default="http://127.0.0.1:8080")
    p.add_argument("--timeout",type=int,default=240)
    p.add_argument("--output",required=True)
    a=p.parse_args()
    run(a)


if __name__=="__main__":
    main()
