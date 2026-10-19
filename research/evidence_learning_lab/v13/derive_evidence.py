#!/usr/bin/env python3
"""Transparent deterministic EVENT-TO-GRAPH adapter + baseline.

The rule-based interpretation is explicitly scored separately. Never load the
fault controller oracle or T2 repair outcome. All sources remain unverified.
"""
import argparse,csv,hashlib,json
from pathlib import Path
ARMS=("H-A","H-B","H-C")
FIELDS=("case_id","cutoff","hypothesis","edge_role","strength",
        "event_id","event_time","source_system","source_sha256")

def decision(events,cutoff):
    relevant=[x for x in events if x["stage"] in (("T0",) if cutoff=="T0" else ("T0","T1"))]
    if cutoff=="T0":return "none","NO_DIAGNOSTIC_SNAPSHOT_YET"
    erp=[x for x in relevant if x["stage"]=="T1" and x["source"]=="erp_adapter"]
    wms=[x for x in relevant if x["stage"]=="T1" and x["source"]=="wms_adapter"]
    if len(erp)!=1 or len(wms)!=1:return "none","MISSING_SOURCE_SNAPSHOT"
    a=erp[0]["payload"];b=wms[0]["payload"]
    # Diagnostic rules know supply-chain concepts, not the injected fault label.
    candidates=[]
    if a["pending_receipts"]>0 and b["actual"]>a["posted"]:
        candidates.append("H-A")
    if b["snapshot_age"]>=20 and b["snapshot"]<b["actual"]:
        candidates.append("H-B")
    if a["reorder_min"]<10 and a["pending_receipts"]==0:
        candidates.append("H-C")
    if len(candidates)!=1:return "none","AMBIGUOUS_OR_UNSUPPORTED_TELEMETRY"
    return candidates[0],"HANDWRITTEN_DIAGNOSTIC_RULE"

def read_public(path,cutoff):
    entries=[json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    ids={x["case_id"] for x in entries}
    if len(ids)!=1:raise ValueError("cross-incident event contamination")
    seen=set()
    for e in entries:
        if e["event_id"] in seen:raise ValueError("duplicate event")
        seen.add(e["event_id"])
        if e["source"] not in ("order_service","erp_adapter","wms_adapter"):
            raise ValueError("unexpected event producer")
        if e["provenance_class"]!="sandbox_self_reported_unverified":
            raise ValueError("fabricated authenticated source")
    selected=[x for x in entries if x["stage"] in (("T0",) if cutoff=="T0" else ("T0","T1"))]
    if len(selected)!=(1 if cutoff=="T0" else 3):
        raise ValueError("incorrect event count at fixed cutoff")
    return selected

def transform(source_root):
    rows=[];baselines=[];receipts=[]
    for case_dir in sorted(source_root.glob("LAB-*")):
        file=case_dir/"events.jsonl"
        for cutoff in ("T0","T1"):
            events=read_public(file,cutoff)
            case_id=events[0]["case_id"]
            chosen,reason=decision(events,cutoff)
            baseline={"case_id":case_id,"cutoff":cutoff,
               "prediction":chosen,"reason":reason,
               "source":"PROJECT_AUTHORED_DETERMINISTIC_RULE_BASELINE"}
            baselines.append(baseline)
            cause=next(e for e in events if e["stage"]=="T0")
            diagnostic_source="wms_adapter" if chosen=="H-B" else "erp_adapter"
            related=next((e for e in events if e["stage"]=="T1" and
                     e["source"]==diagnostic_source),cause)
            for hypothesis in ARMS:
                labels=[("supports",0.35,cause)]
                if cutoff=="T1" and chosen!="none":
                    labels.append(("supports" if hypothesis==chosen else "contradicts",
                                   0.82 if hypothesis==chosen else 0.72,related))
                for role,strength,source in labels:
                    raw=json.dumps(source,sort_keys=True,separators=(",",":"),ensure_ascii=False)
                    rows.append({"case_id":case_id,"cutoff":cutoff,
                       "hypothesis":hypothesis,"edge_role":role,"strength":strength,
                       "event_id":source["event_id"],"event_time":source["event_time"],
                       "source_system":source["source"],
                       "source_sha256":hashlib.sha256(raw.encode()).hexdigest()})
            receipts.append({"case_id":case_id,"cutoff":cutoff,
               "observed_event_ids":[x["event_id"] for x in events],
               "later_events_excluded":cutoff=="T0",
               "t2_repair_outcome_excluded":True,
               "rule_baseline_prediction":chosen,
               "edge_count":3 if cutoff=="T0" else 6,
               "source_authenticated":False})
    if len(baselines)!=12 or len(rows)!=54:raise ValueError("wrong fixed 6x2 population")
    return rows,baselines,receipts

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--public",required=True);p.add_argument("--relations",required=True)
    p.add_argument("--baseline",required=True);p.add_argument("--receipt",required=True)
    a=p.parse_args()
    rows,baselines,receipts=transform(Path(a.public))
    with open(a.relations,"w",newline="") as file:
        w=csv.DictWriter(file,fieldnames=FIELDS,delimiter="\t")
        w.writeheader();w.writerows(rows)
    Path(a.baseline).write_text(json.dumps(baselines,sort_keys=True,indent=2)+"\n")
    Path(a.receipt).write_text(json.dumps(receipts,sort_keys=True,indent=2)+"\n")
    print("v13_event_adapter=PASS cases=6 timepoints=2 native_edges=54 "
      "future_T2_excluded=YES oracle_not_loaded=YES rule_baseline=12")
if __name__=="__main__":main()
