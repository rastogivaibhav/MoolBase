#!/usr/bin/env python3
"""Prepare and evaluate v8 routing with original unmodified F6 native execution.

Evaluator gold is loaded ONLY during scoring, after all arms have emitted
routes and calculator results. This is an internally authored open benchmark.
"""
import argparse
import csv
import json
import math
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from challenge import cards_from
from scorer import oracle
from export_typed import mechanisms,TASK_FIELDS,RECEIPT_FIELDS,write_tsv
from select_baselines import read
from build_trials import QUERY_FIELDS,ROUTE_FIELDS
from build_semantic import load_aliases

def prepare(queries,routes,selected):
    if len(queries)!=120 or len(routes)!=120:
        raise ValueError("wrong v8 trial population")
    if [q["opaque_id"] for q in queries]!=[r["opaque_id"] for r in routes]:
        raise ValueError("different input/selection ordering")
    rows=[]
    for q,r in zip(queries,routes):
        if r["selection_status"]=="abstain":
            if r["mechanism_id"]!="-" or r["domain"]!="-":
                raise ValueError("rejected source still selected a mechanism")
            continue
        if r["selection_status"]!="selected":
            raise ValueError("unrecognized selected result")
        rows.append({
            "task_id":q["opaque_id"],"domain":r["domain"],
            "mechanism_id":r["mechanism_id"],"target":q["target"],
            "target_unit":q["target_unit"],"inputs":q["inputs"]
        })
    write_tsv(selected,TASK_FIELDS,rows)
    return len(rows)

def grade(original,routes,normalized,gold,executed,bydomain,arm):
    if arm not in ("B0","B1","B2"):
        raise ValueError("unknown F8 arm")
    if len(original)!=120 or len(routes)!=120 or len(normalized)!=120:
        raise ValueError("missing trial records")
    truths=gold.get("trials")
    if gold.get("experiment")!="MOOL-POLYMATH-F8-001" or len(truths)!=120:
        raise ValueError("invalid evaluator-only key")
    ids=[q["opaque_id"] for q in original]
    if ids!=[q["opaque_id"] for q in normalized] or ids!=[r["opaque_id"] for r in routes] or ids!=[g["opaque_id"] for g in truths]:
        raise ValueError("different trial identities")
    native={}
    for r in executed:
        tid=r["task_id"]
        if tid in native:
            raise ValueError("repeated native calculation receipt")
        native[tid]=r
    selected_ids={r["opaque_id"] for r in routes
                  if r["selection_status"]=="selected"}
    if set(native)!=selected_ids:
        raise ValueError("selected task missing native execution receipt")
    cases=[]
    for q,r,n,g in zip(original,routes,normalized,truths):
        category=g["category"]
        origin=g["original"]
        known=category=="library"
        if known:
            if not origin:
                raise ValueError("missing source question for library trial")
            expected_domain=origin["domain"]
            mechanism=bydomain[expected_domain]["mechanism_id"]
            expected_number=oracle(origin)
        else:
            if origin is not None:
                raise ValueError("negative case acquired gold mechanism")
            expected_domain=None;mechanism=None;expected_number=None
        chosen=r["selection_status"]=="selected"
        route_correct=((known and chosen and r["domain"]==expected_domain and
                       r["mechanism_id"]==mechanism) or
                       (not known and not chosen))
        calc_status="NO_EXECUTION"
        numeric_correct=False
        refused=not chosen
        if chosen:
            rec=native[r["opaque_id"]]
            if (rec["mechanism_id"]!=r["mechanism_id"] or
                rec["domain"]!=r["domain"] or
                rec["target"]!=n["target"] or
                rec["target_unit"]!=n["target_unit"] or
                rec["source_status"]!="unverified" or
                rec["scientific_truth_claim"]!="false" or
                rec["policy_version"]!="MOOL-F6-TYPED-V1"):
                raise ValueError("native v6 receipt contradicts selected mechanism")
            calc_status=rec["status"]
            if calc_status=="executed_unverified":
                actual=float(rec["value"])
                if not math.isfinite(actual):
                    raise ValueError("nonfinite native numerical result")
                numeric_correct=bool(route_correct and expected_number is not None
                    and math.isclose(actual,expected_number,rel_tol=0,abs_tol=1e-9))
            elif calc_status=="abstain":
                if rec["value"]!="-" or rec["reason"]=="-":
                    raise ValueError("malformed calculator refusal")
                refused=True
            else:
                raise ValueError("unsupported v6 execution result")
        if not known:
            correct=not chosen
        elif expected_number is None:
            correct=bool(route_correct and refused)
        else:
            correct=bool(route_correct and numeric_correct)
        cases.append({
            "opaque_id":q["opaque_id"],"category":category,
            "correct":correct,"route_correct":bool(route_correct),
            "calculator_correct":numeric_correct,"refused":refused,
            "executor_status":calc_status
        })
    def stats(rows):
        if not rows:
            return {"n":0}
        return {"n":len(rows),"correct":sum(c["correct"] for c in rows),
                "routing_correct":sum(c["route_correct"] for c in rows),
                "numeric_correct":sum(c["calculator_correct"] for c in rows),
                "refused":sum(c["refused"] for c in rows),
                "accuracy":sum(c["correct"] for c in rows)/len(rows)}
    return {
        "experiment":"MOOL-POLYMATH-F8-001",
        "arm":arm,
        "report_status":"DETERMINISTIC_AUTHORED_ONTOLOGY_ENGINEERING_NOT_AI_SCIENCE",
        "independently_authored_blind_evaluation":"NOT_RUN",
        "actual_LLM":"NOT_RUN",
        "hypokosh_dwm_in_this_study":"NOT_RUN",
        "scientific_discoveries":0,
        "metrics":stats(cases),
        "categories":{k:stats([c for c in cases if c["category"]==k])
                      for k in ("library","unsupported_unit","conflicting_alias","out_of_domain","composite")},
        "case_receipts":cases
    }

def main():
    p=argparse.ArgumentParser()
    for key in ("queries","normalized","routes","selected_tasks"):
        p.add_argument("--"+key,required=True)
    p.add_argument("--arm",required=True,choices=["B0","B1","B2"])
    p.add_argument("--gold")
    p.add_argument("--executed")
    p.add_argument("--mechanisms")
    p.add_argument("--cards")
    p.add_argument("--out")
    a=p.parse_args()
    original=read(a.queries,QUERY_FIELDS)
    normalized=read(a.normalized,QUERY_FIELDS)
    routes=read(a.routes,ROUTE_FIELDS)
    if not a.gold:
        n=prepare(normalized,routes,a.selected_tasks)
        print(f"f8_{a.arm}_selected=PASS selected={n} rejected={120-n}")
    else:
        if not all([a.executed,a.mechanisms,a.cards,a.out]):
            p.error("scoring requires --executed --mechanisms --cards --out")
        with open(a.executed,encoding="utf-8",newline="") as f:
            reader=csv.DictReader(f,delimiter="\t")
            if reader.fieldnames!=list(RECEIPT_FIELDS):
                raise ValueError("native F6 execution receipt contract changed")
            calculated=list(reader)
        report=grade(original,routes,normalized,json.loads(Path(a.gold).read_text()),
                     calculated,mechanisms(a.mechanisms,cards_from(a.cards)),a.arm)
        Path(a.out).write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
        m=report["metrics"]
        print(f"f8_{a.arm}_score=PASS correct={m['correct']}/120 "
              f"routed={m['routing_correct']} numeric={m['numeric_correct']} "
              "independent_polymath=NOT_TESTED")

if __name__=="__main__":
    main()
