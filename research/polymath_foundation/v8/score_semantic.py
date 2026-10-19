#!/usr/bin/env python3
"""F8 blinded-at-inference scorer: model output + native tool receipts, gold only here."""
import argparse, json, csv, math, hashlib, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from scorer import oracle
from select_baselines import read
from build_trials import ROUTE_FIELDS
from export_typed import RECEIPT_FIELDS

def decode(s):
    out={}
    for entry in s.split(";"):
        if "=" not in entry or "@" not in entry or "#" not in entry:return None
        k,second=entry.split("=",1)
        try:
            val,rest=second.split("@",1)
            unit,typ=rest.split("#",1)
            num=float(val)
            if not math.isfinite(num):return None
            if k in out:return None
            out[k]=(num,unit,typ)
        except (ValueError,TypeError):return None
    return out

def semantic_match(pred,truth):
    if pred is None or truth is None:return False
    if pred["target"]!=truth["target"] or pred["target_unit"]!=truth["target_unit"]:
        return False
    p=decode(pred["inputs"]);g=decode(truth["inputs"])
    if p is None or g is None or p.keys()!=g.keys():return False
    return all(p[k][1:]==g[k][1:] and math.isclose(p[k][0],g[k][0],rel_tol=0,abs_tol=1e-9) for k in g)

def evaluate(predictions,routes,execs,gold,arm):
    if len(predictions["records"])!=len(routes)!=24:
        raise ValueError("invalid frozen question count")
    if len(gold)!=24:raise ValueError("missing gold trial data")
    ids=[t["opaque_id"] for t in gold]
    if ids!=[x["opaque_id"] for x in predictions["records"]] or ids!=[x["opaque_id"] for x in routes]:
        raise ValueError("predictions and sources misaligned")
    execution={r["task_id"]:r for r in execs}
    selected_ids={r["opaque_id"] for r in routes if r["selection_status"]=="selected"}
    if len(execution)!=len(execs) or set(execution)!=selected_ids:
        raise ValueError("not exact set of native execution receipts")
    result=[]
    for g,p,r in zip(gold,predictions["records"],routes):
        resp=p["normalized_response"];typeclass=g["category"]
        semantic=semantic_match(resp["normalized"],g["expected_normalized"])
        parser_failed=bool(resp["parser_error"])
        selection=r["selection_status"]=="selected"
        expected_id=g["mechanism_id"]
        correct_mech=bool(selection and expected_id==r["mechanism_id"] and
                          g["domain"]==r["domain"])
        if arm=="B3" and selection and r["native_node_id"]=="-":
            raise ValueError("fake native node ID in B3")
        number_correct=False;safe_refusal=False;why=r["reason"]
        if selection:
            e=execution[g["opaque_id"]]
            if (e["mechanism_id"]!=r["mechanism_id"] or
                e["source_status"]!="unverified" or
                e["scientific_truth_claim"]!="false" or
                e["policy_version"]!="MOOL-F6-TYPED-V1"):
                raise ValueError("malformed provenance or calculator source")
            if e["status"]=="executed_unverified":
                val=float(e["value"])
                expected=oracle(g["source_task"]) if g["source_task"] is not None else None
                number_correct=bool(expected is not None and correct_mech and
                        semantic and math.isfinite(val) and math.isclose(
                        expected,val,rel_tol=0,abs_tol=1e-9))
            elif e["status"]=="abstain" and e["value"]=="-" and e["reason"]!="-":
                safe_refusal=True
            else:raise ValueError("unsupported native execution format")
            why=e["reason"]
        else:
            safe_refusal=True
        # Numeric correctness, not bare abstention, is necessary on valid cases.
        if typeclass=="in_library_valid":
            passed=bool(not parser_failed and semantic and correct_mech and number_correct)
        elif typeclass=="assumption_violation":
            passed=bool(not parser_failed and semantic and correct_mech and safe_refusal)
        elif typeclass in ("unknown_mechanism","composite"):
            # Cannot get credit for a parsing failure, an incorrectly
            # normalized unsupported request or a numeric error.
            passed=bool(not parser_failed and resp["status"]=="abstain"
                        and not selection and safe_refusal)
        else:raise ValueError("unknown evaluation class")
        result.append({"opaque_id":g["opaque_id"],"class":typeclass,
          "passed":passed,"semantic_exact":semantic,
          "mechanism_correct":correct_mech,"native_calculation_correct":number_correct,
          "native_safe_refusal":safe_refusal,"parse_error":parser_failed,
          "selection_status":r["selection_status"],"reason":why})
    def sums(rows):
        return {"n":len(rows),"passed":sum(x["passed"] for x in rows),
                "exact_normalization":sum(x["semantic_exact"] for x in rows),
                "correct_mechanism":sum(x["mechanism_correct"] for x in rows),
                "numeric_correct":sum(x["native_calculation_correct"] for x in rows),
                "safe_refusal":sum(x["native_safe_refusal"] for x in rows),
                "parse_error":sum(x["parse_error"] for x in rows)}
    return {"experiment":"MOOL-POLYMATH-F8-001","arm":arm,
      "score_type":"NONBLIND_PROJECT_AUTHORED_SEMANTIC_PILOT",
      "real_model_executed":predictions["actual_model_executed"],
      "native_moolbase_execution":arm=="B3",
      "full_dwm_opposition_cycle":"NOT_RUN",
      "independent_blind":False,"new_verified_scientific_facts":0,
      "overall":sums(result),
      "categories":{cat:sums([r for r in result if r["class"]==cat])
                    for cat in ("in_library_valid","assumption_violation",
                                "unknown_mechanism","composite")},
      "case_receipts":result}

def main():
    p=argparse.ArgumentParser()
    for x in ("predictions","routes","native-results","gold","out"):
        p.add_argument("--"+x,required=True)
    p.add_argument("--arm",required=True,choices=("B0","B1","B2","B3"))
    a=p.parse_args()
    preds=json.loads(Path(a.predictions).read_text())
    routes=read(a.routes,ROUTE_FIELDS)
    with open(a.native_results,encoding="utf8",newline="") as f:
        reader=csv.DictReader(f,delimiter="\t")
        if reader.fieldnames!=list(RECEIPT_FIELDS):
            raise ValueError("missing original v6 calculator format")
        execs=list(reader)
    gold=json.loads(Path(a.gold).read_text())
    scored=evaluate(preds,routes,execs,gold,a.arm)
    Path(a.out).write_text(json.dumps(scored,indent=2,sort_keys=True)+"\n")
    print(f"f8_{a.arm}_score=PASS {scored['overall']['passed']}/24 "
          f"native={a.arm=='B3'} genuine_llm={preds['actual_model_executed']}")
if __name__=="__main__":main()
