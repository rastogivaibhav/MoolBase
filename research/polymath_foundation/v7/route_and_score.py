#!/usr/bin/env python3
"""F7 honest native-selected procedure execution and separate gold scoring."""
import argparse
import collections
import csv
import json
import math
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v6"))
from challenge import cards_from,sha
from scorer import oracle
from export_typed import TASK_FIELDS,RECEIPT_FIELDS,write_tsv
from build_trials import QUERY_FIELDS,ROUTE_FIELDS
from select_baselines import read

def prepare(queries,routes,selected_file):
    if len(queries)!=108 or len(routes)!=108:
        raise ValueError("frozen 108-question trial count invalid")
    qids=[q["opaque_id"] for q in queries]
    if qids!=[r["opaque_id"] for r in routes] or len(set(qids))!=108:
        raise ValueError("routing changed question order or identity")
    selected=[]
    for q,r in zip(queries,routes):
        if r["selection_status"]=="abstain":
            if r["mechanism_id"]!="-" or r["domain"]!="-":
                raise ValueError("refused candidate retained a mechanism ID")
            continue
        if r["selection_status"]!="selected":
            raise ValueError("unrecognized route outcome")
        if r["mechanism_id"]=="-" or r["domain"]=="-":
            raise ValueError("unidentified selected mechanism")
        selected.append({
            "task_id":q["opaque_id"],"domain":r["domain"],
            "mechanism_id":r["mechanism_id"],"target":q["target"],
            "target_unit":q["target_unit"],"inputs":q["inputs"]
        })
    if not selected:
        raise ValueError("no accepted trials; no native executor evidence")
    write_tsv(selected_file,TASK_FIELDS,selected)
    return len(selected)

def score(queries,routes,gold,executed,bydomain,arm):
    if arm not in ("B0","B1","B2"):
        raise ValueError("unknown selection baseline")
    truths=gold.get("trials")
    if (gold.get("experiment")!="MOOL-POLYMATH-F7-001"
        or gold.get("gold_status")!="evaluation_only_not_for_selection"
        or len(truths)!=108 or len(queries)!=108 or len(routes)!=108):
        raise ValueError("gold/route corpus mismatched")
    if ([q["opaque_id"] for q in queries]!=
        [r["opaque_id"] for r in routes] or
        [g["opaque_id"] for g in truths]!=
        [q["opaque_id"] for q in queries]):
        raise ValueError("different shuffled trial sets")
    output={}
    for rec in executed:
        tid=rec["task_id"]
        if tid in output:
            raise ValueError("duplicate C++ calculator receipt")
        output[tid]=rec
    if set(output)!={r["opaque_id"] for r in routes
                    if r["selection_status"]=="selected"}:
        raise ValueError("missing native calculator record or unexpected output")
    results=[]
    for q,r,g in zip(queries,routes,truths):
        category=g["category"]
        original=g["source_task"]
        expected_domain=g["expected_domain"]
        library=(category=="library")
        if library:
            if not isinstance(original,dict) or not expected_domain:
                raise ValueError("missing original benchmark mapping")
            correct_mech=bydomain[expected_domain]["mechanism_id"]
            expected_value=oracle(original)
        else:
            if original is not None or expected_domain is not None:
                raise ValueError("negative control incorrectly assigned mechanism")
            correct_mech=None
            expected_value=None
        chosen=r["selection_status"]=="selected"
        proper_selection=(chosen and library and
                          r["mechanism_id"]==correct_mech and
                          r["domain"]==expected_domain) or (not chosen and not library)
        numeric=False
        supported=False
        refusal=False
        reason=r["reason"]
        if chosen:
            c=output[q["opaque_id"]]
            if (c["mechanism_id"]!=r["mechanism_id"] or
                c["domain"]!=r["domain"] or
                c["target"]!=q["target"] or
                c["target_unit"]!=q["target_unit"] or
                c["scientific_truth_claim"]!="false" or
                c["source_status"]!="unverified" or
                c["policy_version"]!="MOOL-F6-TYPED-V1"):
                raise ValueError("actual native executor provenance mismatch")
            status=c["status"]
            if status=="executed_unverified":
                numeric=True
                if c["value"]=="-" or c["reason"]!="-":
                    raise ValueError("native number missing or reason forged")
                value=float(c["value"])
                if not math.isfinite(value):
                    raise ValueError("nonfinite native calculator output")
                supported=bool(library and proper_selection and
                               expected_value is not None and
                               math.isclose(value,expected_value,rel_tol=0,abs_tol=1e-9))
            elif status=="abstain":
                if c["value"]!="-" or c["reason"]=="-":
                    raise ValueError("native abstention includes unsupported numeric")
                refusal=True
                reason=c["reason"]
            else:
                raise ValueError("unknown native executor status")
        else:
            refusal=True
        if library:
            if expected_value is None:
                correct=bool(proper_selection and refusal)
            else:
                correct=bool(proper_selection and supported)
        else:
            correct=bool(not chosen and refusal)
        results.append({
            "opaque_id":q["opaque_id"],"category":category,
            "expected_mechanism":correct_mech,"selected":chosen,
            "correct_mechanism_decision":bool(proper_selection),
            "calculation_correct":supported,
            "safe_refusal":refusal,"numeric_emitted":numeric,
            "correct":correct,"refusal_reason":reason
        })
    def summarize(subset):
        n=len(subset)
        return {
            "n":n,"correct_n":sum(x["correct"] for x in subset),
            "end_to_end_accuracy":sum(x["correct"] for x in subset)/n if n else None,
            "mechanism_decision_correct_n":sum(x["correct_mechanism_decision"] for x in subset),
            "mechanism_decision_accuracy":sum(x["correct_mechanism_decision"] for x in subset)/n if n else None,
            "numerical_correct_n":sum(x["calculation_correct"] for x in subset),
            "numeric_emitted_n":sum(x["numeric_emitted"] for x in subset),
            "safe_refusal_n":sum(x["safe_refusal"] for x in subset)
        }
    return {
        "experiment":"MOOL-POLYMATH-F7-001",
        "arm":arm,
        "claim":"CONTROLLED_PROCEDURE_ROUTING_NOT_INTELLIGENT_POLYMATH",
        "independent_blind":False,
        "learner_model_executed":False,
        "hypokosh_dwm_executed_in_this_study":False,
        "new_verified_facts":0,
        "metrics":summarize(results),
        "categories":{k:summarize([r for r in results if r["category"]==k])
                      for k in ("library","poison_unit","out_of_domain","cross_domain_composite")},
        "case_receipts":results,
        "limitations":[
            "The public benchmark retains unique, preauthored input signatures.",
            "No learned model or blind expert mechanism-selection task was run.",
            "All numerical solutions use the original predefined v6 native calculator.",
            "B2 native MoolBase uses the same selection algorithm as B1 baseline.",
            "Data and ontology status are project-authored and unverified."
        ]
    }

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--queries",required=True)
    p.add_argument("--routes",required=True)
    p.add_argument("--arm",required=True,choices=["B0","B1","B2"])
    p.add_argument("--selected-tasks",required=True)
    p.add_argument("--gold")
    p.add_argument("--executor-results")
    p.add_argument("--mechanisms")
    p.add_argument("--cards")
    p.add_argument("--out")
    a=p.parse_args()
    queries=read(a.queries,QUERY_FIELDS)
    routes=read(a.routes,ROUTE_FIELDS)
    if a.gold is None:
        n=prepare(queries,routes,a.selected_tasks)
        print(f"f7_{a.arm}_native_execution_input=PASS selected={n}")
    else:
        if not all([a.executor_results,a.mechanisms,a.cards,a.out]):
            p.error("score mode requires --executor-results --mechanisms --cards --out")
        sys.path.insert(0,str(HERE.parent/"v6"))
        from export_typed import mechanisms
        gold=json.loads(Path(a.gold).read_text())
        bydomain=mechanisms(a.mechanisms,cards_from(a.cards))
        execs=read(a.executor_results,RECEIPT_FIELDS)
        report=score(queries,routes,gold,execs,bydomain,a.arm)
        Path(a.out).write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
        m=report["metrics"]
        print(f"f7_{a.arm}_score=PASS correct={m['correct_n']}/108 "
              f"decisions={m['mechanism_decision_correct_n']}/108 "
              f"numeric_correct={m['numerical_correct_n']} "
              "blind_science=NOT_TESTED")
if __name__=="__main__":
    main()
