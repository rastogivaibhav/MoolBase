#!/usr/bin/env python3
"""Score real native F6 C++ receipts using the original, independent v3 oracle.

No model was run. Exact maths on toy procedures is NOT acquired comprehension.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import sys

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
from challenge import cards_from,generate,sha
from scorer import oracle
from export_typed import SEED,mechanisms,RECEIPT_FIELDS

def load_receipts(path):
    with open(path,encoding="utf-8",newline="") as f:
        rows=csv.DictReader(f,delimiter="\t")
        if rows.fieldnames!=list(RECEIPT_FIELDS):
            raise ValueError("native receipt schema does not match frozen contract")
        return list(rows)

def evaluate(cards,bydomain,rows):
    master=generate(cards,SEED)
    questions=master["tasks"]
    if len(rows)!=len(questions)!=72:
        raise ValueError("missing native task receipts")
    by_id={}
    for r in rows:
        tid=r["task_id"]
        if tid in by_id or not tid:
            raise ValueError("duplicate or missing native task ID")
        by_id[tid]=r
    if set(by_id)!={t["task_id"] for t in questions}:
        raise ValueError("native receipt identity differs from challenge")
    details=[]
    for t in questions:
        r=by_id[t["task_id"]]
        mechanism=bydomain[t["domain"]]
        expected=oracle(t)
        boundary=expected is None
        if (r["domain"]!=t["domain"] or
            r["mechanism_id"]!=mechanism["mechanism_id"] or
            r["target"]!=t["target"] or
            r["target_unit"]!=dict((x[0],x[2]) for x in mechanism["output_fields"])[t["target"]] or
            r["source_family"]!=mechanism["source_family"] or
            r["source_status"]!="unverified" or
            r["policy_version"]!="MOOL-F6-TYPED-V1" or
            r["scientific_truth_claim"]!="false" or
            r["native_node_id"]=="-"):
            raise ValueError("native graph provenance, target or citation mismatch")
        status=r["status"]
        if status not in ("executed_unverified","abstain"):
            raise ValueError("unknown native execution status")
        if status=="executed_unverified":
            if r["reason"]!="-" or r["value"]=="-":
                raise ValueError("numeric answer malformed")
            try:
                actual=float(r["value"])
            except ValueError:
                raise ValueError("numeric answer not parseable")
            if not math.isfinite(actual):
                raise ValueError("nonfinite native answer")
            correct=not boundary and math.isclose(actual,expected,rel_tol=0,abs_tol=1e-9)
        else:
            if r["value"]!="-" or not r["reason"] or r["reason"]=="-":
                raise ValueError("abstention must name a boundary")
            correct=boundary
        details.append({
            "task_id":t["task_id"],"domain":t["domain"],"boundary":boundary,
            "correct":correct,"status":status,"boundary_reason":r["reason"],
            "mechanism_id":r["mechanism_id"],
            "source_status":r["source_status"],
            "native_node_id":r["native_node_id"]
        })
    def summarize(xs):
        valid=[r for r in xs if not r["boundary"]]
        boundary=[r for r in xs if r["boundary"]]
        return {
            "n":len(xs),"valid_cases":len(valid),"boundary_cases":len(boundary),
            "correct":sum(r["correct"] for r in xs),
            "total_accuracy":sum(r["correct"] for r in xs)/len(xs),
            "application_accuracy":sum(r["correct"] for r in valid)/len(valid),
            "boundary_abstention":sum(r["correct"] for r in boundary)/len(boundary),
            "unsupported_numeric":sum(
                r["status"]=="executed_unverified" and not r["correct"]
                for r in xs)
        }
    return {
        "experiment":"MOOL-POLYMATH-F6-001",
        "score_type":"BOUNDED_DETERMINISTIC_PROCEDURE_VALIDATION_NOT_AI_LEARNING",
        "learner_executed":False,
        "autonomous_mechanism_selection":"NOT_RUN",
        "verified_scientific_facts":0,
        "task_source":"public_pedagogical_generator_unblinded",
        "question_sha256":sha(master),
        "metrics":summarize(details),
        "per_domain":{d:summarize([r for r in details if r["domain"]==d])
                      for d in ("networks","biology","supply_chain")},
        "case_receipts":details,
        "limitations":[
            "Pre-authored mechanisms are allowlisted; no model inferred or selected them.",
            "Python scorer and C++ executor implement the same public toy equations separately.",
            "Questions are procedurally generated from public tasks, not independently held back.",
            "Deterministic code accuracy says nothing about external scientific truth.",
            "Source cards remain project-authored and unverified."
        ]
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--mechanisms",required=True)
    p.add_argument("--native",required=True)
    p.add_argument("--out",required=True)
    args=p.parse_args()
    cards=cards_from(args.cards)
    source=mechanisms(args.mechanisms,cards)
    rows=load_receipts(args.native)
    out=evaluate(cards,source,rows)
    Path(args.out).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n",
                              encoding="utf-8")
    v=out["metrics"]
    print(f"f6_scoring=PASS correct={v['correct']}/{v['n']} "
          f"valid={v['application_accuracy']:.3f} "
          f"boundary={v['boundary_abstention']:.3f} "
          f"verified_science=0 model_learned=NOT_RUN")

if __name__=="__main__":
    main()
