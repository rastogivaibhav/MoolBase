#!/usr/bin/env python3
"""V14 neutral, label-free ERP/WMS field observations; NEVER call v13 decision().

Source data are internally generated, unverified SQLite events. This adapter
cannot discover a cause: it only exposes raw numeric source measurements.
"""
import argparse,csv,hashlib,json
from pathlib import Path
FIELDS=("case_id","cutoff","event_id","event_time","source_system","field_name","value","source_sha256")
SOURCES={"T0":{"order_service"},"T1":{"order_service","erp_adapter","wms_adapter"}}
EXPECTED_FIELDS={"T0":{"open_orders","unavailable_units"},
                 "T1":{"open_orders","unavailable_units","posted","pending_receipts","reorder_min","open_orders",
                       "actual","snapshot","snapshot_age"}}
FORBIDDEN_KEYS={"injected_fault","injected_cause","fault","ground_truth","oracle","repair","intervention"}
def canonical(x):return json.dumps(x,sort_keys=True,separators=(",",":"),ensure_ascii=False)
def fields_at(public_path,cutoff):
    events=[json.loads(x) for x in public_path.read_text().splitlines() if x.strip()]
    selected=[]
    seen=set()
    for e in events:
        if not isinstance(e,dict):raise ValueError("malformed operational event")
        if e.get("stage") not in ("T0","T1","T2"):raise ValueError("invalid stage")
        if e.get("provenance_class")!="sandbox_self_reported_unverified":raise ValueError("self-attested authority")
        if e.get("event_id") in seen:raise ValueError("duplicate event ID")
        seen.add(e.get("event_id"))
        if any(k in e for k in FORBIDDEN_KEYS):raise ValueError("oracle/repair control field leaked")
        if e["stage"]=="T2":continue  # post-repair observation withheld unconditionally
        if cutoff=="T0" and e["stage"]!="T0":continue
        if e["source"] not in SOURCES[cutoff]:raise ValueError("unexpected producer")
        if e["stage"]=="T0" and e["source"]!="order_service":raise ValueError("T0 source corruption")
        if e["stage"]=="T1" and e["source"] not in ("erp_adapter","wms_adapter"):raise ValueError("T1 source corruption")
        if not isinstance(e.get("payload"),dict):raise ValueError("missing telemetry payload")
        for k in e["payload"]:
            if k in FORBIDDEN_KEYS:raise ValueError("fault label in telemetry")
        if e["stage"]=="T1" and not str(e.get("event_time","")).startswith("2026-10-19T09:10:"):
            raise ValueError("unexpected T1 time")
        if e["stage"]=="T0" and not str(e.get("event_time","")).startswith("2026-10-19T09:00:"):
            raise ValueError("unexpected T0 time")
        selected.append(e)
    if len(selected)!=(1 if cutoff=="T0" else 3):raise ValueError("missing/extra time-cutoff event")
    if {e["source"] for e in selected}!=SOURCES[cutoff]:raise ValueError("incomplete source-snapshot set")
    rows=[]
    for e in selected:
        digest=hashlib.sha256(canonical(e).encode("utf8")).hexdigest()
        for key,value in sorted(e["payload"].items()):
            if isinstance(value,bool):raise ValueError("boolean measurement unsupported")
            if isinstance(value,int):
                if value<0:raise ValueError("invalid negative inventory measurement")
                rows.append({"case_id":e["case_id"],"cutoff":cutoff,
                   "event_id":e["event_id"],"event_time":e["event_time"],
                   "source_system":e["source"],"field_name":key,"value":str(value),
                   "source_sha256":digest})
            elif key!="exception":
                raise ValueError("unrecognized nonnumeric measurement")
    if len(rows)!=(2 if cutoff=="T0" else 9):
        raise ValueError("incomplete numeric observational field set")
    if any(t in canonical(rows).lower() for t in ('"hypothesis"','"h-a"','"h-b"','"h-c"','"supports"','"contradicts"','"injected_fault"')):
        raise ValueError("adapter emitted causal/hypothesis answer")
    return rows,selected
def convert(public):
    root=Path(public)
    case_paths=sorted(root.glob("LAB-*/events.jsonl"))
    if len(case_paths)!=6:raise ValueError("requires exactly six frozen incident event streams")
    allrows=[];receipts=[]
    for path in case_paths:
        for stage in ("T0","T1"):
            items,source_events=fields_at(path,stage)
            if any(x["case_id"]!=path.parent.name for x in items):raise ValueError("cross-case telemetry leakage")
            allrows+=items
            receipts.append({"case_id":path.parent.name,"cutoff":stage,
               "sources":sorted({x["source"] for x in source_events}),
               "numeric_fields":len(items),
               "event_ids":[x["event_id"] for x in source_events],
               "full_event_file_sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
               "oracle_accessed":False,"future_T2_used":False,
               "cause_labels_emitted":False})
    if len(allrows)!=66:raise ValueError("unregistered total observation count")
    return allrows,receipts
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--public",required=True);p.add_argument("--measurements",required=True);p.add_argument("--receipts",required=True)
    a=p.parse_args()
    values,receipts=convert(a.public)
    with open(a.measurements,"w",newline="") as out:
        w=csv.DictWriter(out,fieldnames=FIELDS,delimiter="\t",lineterminator="\n")
        w.writeheader();w.writerows(values)
    Path(a.receipts).write_text(json.dumps(receipts,indent=2,sort_keys=True)+"\n")
    print("v14_neutral_adapter=PASS cases=6 snapshots=12 numeric_fields=66 causal_edges=0 oracle=NOT_ACCESSED")
if __name__=="__main__":main()
