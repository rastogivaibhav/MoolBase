#!/usr/bin/env python3
"""Reproducible, UNAUTHENTICATED operational source fixture; not a reasoning benchmark."""
import argparse,hashlib,json
from pathlib import Path

PRODUCERS=("erp","wms")
EVENT_TYPES={"ledger_snapshot","warehouse_snapshot","receipt_ack","policy_audit"}
PROTECTED=("injected_cause","ground_truth","true_cause","fault_type","correct_hypothesis","adjudicated_cause")
SEED=20261019
FAULTS=(
 ("case-101","receipt_posting_delay"),
 ("case-102","wms_snapshot_staleness"),
 ("case-103","replenishment_policy_misconfiguration"),
)
def canonical(obj):
    return json.dumps(obj,sort_keys=True,separators=(",",":"),ensure_ascii=False).encode("utf-8")
def digest(obj):
    return hashlib.sha256(canonical(obj)).hexdigest()
def event(case,phase,producer,kind,minute,data,seq):
    row={"event_id":f"evt-{seq:03}","case_id":case,
         "phase":phase,"producer_id":producer,"type":kind,
         "sku":"COS-00017",
         "event_time":f"2026-10-19T10:{minute:02}:00Z",
         "recorded_at":f"2026-10-19T10:{minute+1:02}:00Z",
         "payload":data,"origin_authority":"project_authored_synthetic_unverified"}
    return {**row,"sha256":digest(row)}
def make():
    rows=[];truth=[];seq=0
    for ix,(case,cause) in enumerate(FAULTS):
        base=2+ix*14
        queue=[
            ("T0","erp","ledger_snapshot",base,{"reported_on_hand":100,"open_orders":18}),
            ("T0","wms","warehouse_snapshot",base+1,{"reported_on_hand":85,"open_orders":18})
        ]
        if cause=="receipt_posting_delay":
            queue+= [
             ("T1","wms","receipt_ack",base+3,{"receipt_units":15,"receipt_status":"received"}),
             ("T1","erp","receipt_ack",base+4,{"receipt_units":15,"receipt_status":"not_posted"})]
        elif cause=="wms_snapshot_staleness":
            queue+= [
             ("T1","erp","ledger_snapshot",base+3,{"reported_on_hand":100,"last_update_minute":base+3}),
             ("T1","wms","warehouse_snapshot",base+4,{"reported_on_hand":85,"last_update_minute":base-6})]
        else:
            queue+= [
             ("T1","erp","policy_audit",base+3,{"reorder_threshold":120,"approved_threshold":60}),
             ("T1","wms","warehouse_snapshot",base+4,{"reported_on_hand":85,"observed_policy_alert":True})]
        for phase,producer,kind,minute,payload in queue:
            seq+=1
            rows.append(event(case,phase,producer,kind,minute,payload,seq))
        truth.append({"case_id":case,"injected_cause":cause,
                      "controller_timestamp":f"2026-10-19T10:{base:02}:00Z",
                      "synthetic_experimenter":"internal_fixture"})
    return rows,{"experiment":"MOOL-EVIDENCE-LAB-V13-001",
                 "inference_access":"WITHHELD",
                 "external_adjudication":False,"cases":truth}
def validate(events):
    if len(events)<12:raise ValueError("INSUFFICIENT_EVENT_FEED")
    ids=set();cases={};last_time={}
    for row in events:
        expected={"event_id","case_id","phase","producer_id","type","sku",
                  "event_time","recorded_at","payload","origin_authority","sha256"}
        if type(row)!=dict or set(row)!=expected:
            raise ValueError("INVALID_EVENT_SCHEMA")
        if row["event_id"] in ids:raise ValueError("DUPLICATE_EVENT_ID")
        ids.add(row["event_id"])
        if row["producer_id"] not in PRODUCERS:raise ValueError("UNTRUSTED_PRODUCER_NAME")
        if row["type"] not in EVENT_TYPES:raise ValueError("INVALID_EVENT_TYPE")
        if row["phase"] not in ("T0","T1"):raise ValueError("INVALID_EVENT_PHASE")
        if row["origin_authority"]!="project_authored_synthetic_unverified":
            raise ValueError("FORGED_EVIDENCE_AUTHORITY")
        if type(row["payload"])!=dict:raise ValueError("INVALID_PAYLOAD")
        if any(k in PROTECTED for k in row["payload"]) or any(k in row for k in PROTECTED):
            raise ValueError("LEAKED_GROUND_TRUTH")
        import datetime
        try:
            t=datetime.datetime.fromisoformat(row["event_time"].replace("Z","+00:00"))
            recorded=datetime.datetime.fromisoformat(row["recorded_at"].replace("Z","+00:00"))
        except (ValueError,TypeError):raise ValueError("INVALID_TIMESTAMP")
        if recorded<t:raise ValueError("RECORDED_BEFORE_EVENT")
        pair=(row["case_id"],row["producer_id"])
        if pair in last_time and t<=last_time[pair]:raise ValueError("NONMONOTONIC_SOURCE_TIME")
        last_time[pair]=t
        if row["sha256"]!=digest({k:v for k,v in row.items() if k!="sha256"}):
            raise ValueError("CONTENT_INTEGRITY_MISMATCH")
        cases.setdefault(row["case_id"],{}).setdefault(row["phase"],[]).append(t)
    if len(cases)<3:raise ValueError("INSUFFICIENT_INCIDENTS")
    for case,phases in cases.items():
        if not all(phases.get(k) for k in ("T0","T1")):raise ValueError("MISSING_PHASE")
        if min(phases["T1"])<=max(phases["T0"]):
            raise ValueError("FUTURE_EVIDENCE_AT_T0")
    return {"experiment":"MOOL-EVIDENCE-LAB-V13-001",
            "validated":True,"incident_count":len(cases),"event_count":len(events),
            "external_authentication":"NOT_PERFORMED",
            "native_reasoning":"NOT_RUN","llm_baselines":"NOT_RUN",
            "outcome_learning":"NOT_DEMONSTRATED"}
def write(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=True)
    rows,truth=make();report=validate(rows)
    source=root/"observations.jsonl"
    source.write_bytes(b"".join(canonical(e)+b"\n" for e in rows))
    private=root/"evaluator_only_truth.json"
    private.write_bytes(canonical(truth)+b"\n")
    report["observation_file_sha256"]=hashlib.sha256(source.read_bytes()).hexdigest()
    report["withheld_file_sha256"]=hashlib.sha256(private.read_bytes()).hexdigest()
    (root/"validation.json").write_bytes(canonical(report)+b"\n")
    print("v13_source_event_integrity=PASS",report)
def main():
    a=argparse.ArgumentParser();a.add_argument("--output",required=True)
    write(a.parse_args().output)
if __name__=="__main__":main()
