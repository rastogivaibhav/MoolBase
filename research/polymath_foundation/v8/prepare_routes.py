#!/usr/bin/env python3
"""Convert honest semantic guesses to native routing and old C++ calculator TSVs."""
import argparse,csv,json,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from export_typed import MANIFEST_FIELDS,TASK_FIELDS,RECEIPT_FIELDS,write_tsv
from build_trials import QUERY_FIELDS,ROUTE_FIELDS
from select_baselines import read,select

SENTINEL={"description":"Refused prior to matching","target":"unknown",
          "target_unit":"unknown_unit","inputs":"unrecognized=0@unknown_unit#integer"}

def queries_from(predictions):
    records=predictions["records"]
    if len(records)!=24 or len({r["opaque_id"] for r in records})!=24:
        raise ValueError("actual predictions incomplete")
    queries=[]
    for row in records:
        obj=row["normalized_response"]
        normalized=obj["normalized"]
        if obj["status"]=="normalize" and normalized and not obj["parser_error"]:
            data=normalized
        else:data=SENTINEL
        queries.append({"opaque_id":row["opaque_id"],**data})
    return queries

def prepare(routes,queries,path):
    if len(routes)!=24 or [r["opaque_id"] for r in routes]!=[q["opaque_id"] for q in queries]:
        raise ValueError("route identities incomplete or reordered")
    selected=[]
    for r,q in zip(routes,queries):
        if r["selection_status"]=="selected":
            selected.append({"task_id":q["opaque_id"],
              "domain":r["domain"],"mechanism_id":r["mechanism_id"],
              "target":q["target"],"target_unit":q["target_unit"],"inputs":q["inputs"]})
    write_tsv(path,TASK_FIELDS,selected)
    return len(selected)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--predictions",required=True)
    p.add_argument("--manifest",required=True)
    p.add_argument("--queries",required=True)
    p.add_argument("--routes")
    p.add_argument("--selected")
    p.add_argument("--mode",required=True,choices=("queries","baseline","selected"))
    a=p.parse_args()
    preds=json.loads(Path(a.predictions).read_text())
    queries=queries_from(preds)
    if a.mode=="queries":
        write_tsv(a.queries,QUERY_FIELDS,queries)
        print("f8_normalized_query_receipt=PASS n=24")
        return
    if a.mode=="baseline":
        definitions=read(a.manifest,MANIFEST_FIELDS)
        routes=select(queries,definitions,"B1")
        write_tsv(a.routes,ROUTE_FIELDS,routes)
        print("f8_schema_route=PASS selected="+str(sum(x["selection_status"]=="selected" for x in routes)))
        return
    routes=read(a.routes,ROUTE_FIELDS)
    count=prepare(routes,queries,a.selected)
    print(f"f8_frozen_cxx_execution_input=PASS selected={count}")
if __name__=="__main__":main()
