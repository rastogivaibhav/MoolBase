#!/usr/bin/env python3
"""Execute actual SQL ERP and WMS state transitions to produce event streams.

This is a controlled SQLite sandbox, NOT ERPNext, a real warehouse feed, or
independently adjudicated field evidence. The cause oracle is written separately.
"""
import argparse,hashlib,json,sqlite3
from pathlib import Path

BASE_TIME="2026-10-19T09:"
CAUSES=("pending_receipt","stale_snapshot","reorder_rule")
def canonical(obj):
    return json.dumps(obj,sort_keys=True,separators=(",",":"),ensure_ascii=False)
def initialise(path,sql):
    con=sqlite3.connect(path)
    con.execute("PRAGMA journal_mode=WAL")
    con.executescript(sql)
    con.commit()
    return con
def read(con):
    return {k:v for k,v in con.execute("SELECT key, value FROM state ORDER BY key")}
def update(con,values):
    with con:
        for k,v in values.items():
            con.execute("UPDATE state SET value=? WHERE key=?",(v,k))
    return read(con)
def emit(log,case_id,stage,source,payload,minute):
    record={"case_id":case_id,"stage":stage,"event_id":f"{case_id}-{stage}-{len(log)+1:02d}",
      "event_time":f"2026-10-19T09:{minute:02d}:00Z",
      "source":source,"event_type":"inventory_status",
      "payload":payload,"provenance_class":"sandbox_self_reported_unverified"}
    log.append(record)

def execute(case_number,public,secret):
    variant=case_number%2
    fault=CAUSES[case_number//2]
    case_id=f"LAB-{case_number+1:03d}"
    case_path=public/case_id
    case_path.mkdir(parents=True,exist_ok=True)
    erp=initialise(case_path/"erp.sqlite",
      "CREATE TABLE state(key TEXT PRIMARY KEY,value INTEGER NOT NULL);"
      "INSERT INTO state VALUES('posted',100),('pending_receipts',0),"
      "('reorder_min',15),('open_orders',10);")
    wms=initialise(case_path/"wms.sqlite",
      "CREATE TABLE state(key TEXT PRIMARY KEY,value INTEGER NOT NULL);"
      "INSERT INTO state VALUES('actual',100),('snapshot',100),"
      "('snapshot_age',0);")
    trace=[]
    # Variation changes magnitude; experiment type remains hidden from exported logs.
    magnitude=12+variant*4
    if fault=="pending_receipt":
        update(erp,{"pending_receipts":magnitude})
        update(wms,{"actual":100+magnitude,"snapshot":100+magnitude})
    elif fault=="stale_snapshot":
        update(wms,{"snapshot":100-magnitude,"snapshot_age":30+variant*7})
    else:
        update(erp,{"reorder_min":0 if variant==0 else 2})
    # T0: generic symptom with no diagnostic fields for any fault type.
    emit(trace,case_id,"T0","order_service",
      {"open_orders":10,"unavailable_units":magnitude,"exception":"unfulfilled_orders"},
      0)
    # T1: measured state from the actual SQLite stores, not desired cause label.
    emit(trace,case_id,"T1","erp_adapter",read(erp),10)
    emit(trace,case_id,"T1","wms_adapter",read(wms),10)
    observed_before={"erp":read(erp),"wms":read(wms)}
    if fault=="pending_receipt":
        new_stock=erp.execute("SELECT value FROM state WHERE key='posted'").fetchone()[0]+magnitude
        update(erp,{"posted":new_stock,"pending_receipts":0})
    elif fault=="stale_snapshot":
        actual=wms.execute("SELECT value FROM state WHERE key='actual'").fetchone()[0]
        update(wms,{"snapshot":actual,"snapshot_age":0})
    else:
        update(erp,{"reorder_min":15})
    current_erp=read(erp);current_wms=read(wms)
    repaired=(current_erp["pending_receipts"]==0 and
              current_erp["posted"]==current_wms["actual"] and
              current_wms["snapshot"]==current_wms["actual"] and
              current_erp["reorder_min"]>=10)
    if not repaired:raise AssertionError("repair did not reconcile simulated stores")
    emit(trace,case_id,"T2","erp_adapter",current_erp,20)
    emit(trace,case_id,"T2","wms_adapter",current_wms,20)
    erp.close();wms.close()
    (case_path/"events.jsonl").write_text("".join(canonical(e)+"\n" for e in trace))
    oracle={
        "case_id":case_id,"injected_fault":fault,
        "internal_controller_label_only":True,
        "independent_external_adjudication":False,
        "outcome_repaired":repaired,"t1_before":observed_before,
        "t2_after":{"erp":current_erp,"wms":current_wms},
        "scenario_variant":variant
    }
    (secret/f"{case_id}.oracle.json").write_text(json.dumps(oracle,indent=2,sort_keys=True)+"\n")
    return {"case_id":case_id,"t0_events":1,"t1_events":2,
            "t2_events":2,"sql_transactions_executed":True,
            "public_event_hash":hashlib.sha256((case_path/"events.jsonl").read_bytes()).hexdigest()}
def main():
    a=argparse.ArgumentParser()
    a.add_argument("--public",required=True);a.add_argument("--oracle",required=True)
    args=a.parse_args()
    public=Path(args.public);secret=Path(args.oracle)
    if public.resolve()==secret.resolve() or public in secret.parents or secret in public.parents:
        raise ValueError("oracle cannot be in public data tree")
    public.mkdir(parents=True,exist_ok=True);secret.mkdir(parents=True,exist_ok=True)
    rows=[execute(i,public,secret) for i in range(6)]
    (public/"generation_receipt.json").write_text(json.dumps({
        "experiment":"MOOL-EVIDENCE-LEARNING-V13-001",
        "real_sqlite_sql_transactions":True,
        "actual_erpnext_connection":False,
        "source_authentication":False,
        "cases":rows},indent=2)+"\n")
    print("v13_sandbox_sql_transactions=PASS cases=6 repair_outcomes=6 "
          "public_event_streams_separate_from_controller_oracle=YES")
if __name__=="__main__":main()
