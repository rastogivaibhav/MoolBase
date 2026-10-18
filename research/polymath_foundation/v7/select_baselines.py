#!/usr/bin/env python3
"""F7 B0 lexical and B1 strong deterministic schema-only routing baselines.

Neither arm uses an LLM, native DB, evaluator gold, or v3 oracle.
"""
import argparse
import collections
import csv
import math
import re
from pathlib import Path
import sys

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v6"))
from export_typed import MANIFEST_FIELDS,write_tsv
from build_trials import QUERY_FIELDS,ROUTE_FIELDS


def read(path,fields):
    with open(path,encoding="utf-8",newline="") as f:
        reader=csv.DictReader(f,delimiter="\t")
        if reader.fieldnames!=list(fields):
            raise ValueError("unexpected TSV contract for "+str(path))
        rows=list(reader)
    if not rows:
        raise ValueError("empty selection data")
    return rows


def split_schema(s):
    fields={}
    for x in s.split("|"):
        parts=x.split(":")
        if len(parts)!=3 or parts[0] in fields:
            raise ValueError("invalid mechanism schema")
        fields[parts[0]]=(parts[1],parts[2])
    return fields


def decode_inputs(s):
    fields={}
    for token in s.split(";"):
        pair=token.split("=",1)
        if len(pair)!=2 or "@" not in pair[1] or "#" not in pair[1]:
            return None
        name=pair[0]
        value,unit_type=pair[1].split("@",1)
        unit,kind=unit_type.split("#",1)
        if name in fields or not name or not value:
            return None
        fields[name]=(kind,unit)
    return fields


def schema_select(row,definitions):
    provided=decode_inputs(row["inputs"])
    if provided is None or not provided:
        return None,"MALFORMED_INPUT"
    eligible=[]
    for d in definitions:
        required=split_schema(d["input_spec"])
        targets=split_schema(d["output_spec"])
        target=targets.get(row["target"])
        if target is None or target[1]!=row["target_unit"]:
            continue
        # All supplied variables must belong to ONE schema. Missing exactly
        # one is admissible, and the v6 executor must refuse to calculate it.
        if not set(provided).issubset(required):
            continue
        if len(provided)<len(required)-1:
            continue
        if any(provided[name]!=required[name] for name in provided):
            continue
        eligible.append(d)
    if len(eligible)==1:
        return eligible[0],"SCHEMA_UNIQUE"
    if len(eligible)>1:
        return None,"AMBIGUOUS_SCHEMA"
    return None,"NO_APPLICABLE_SCHEMA"


def lexical_tokens(s):
    stop={"a","an","the","a","to","for","of","is","on","or",
          "in","and","may","be","at","from","not","this","that",
          "with","are","under","result","some","it","one","two"}
    return set(x for x in re.findall(r"[a-z0-9]+",s.lower())
               if x not in stop and len(x)>2)


def lexical_select(row,definitions):
    tokens=lexical_tokens(row["description"])
    ranked=[]
    for d in definitions:
        concepts=lexical_tokens(d["description"])
        overlap=len(tokens & concepts)
        ranked.append((overlap,d))
    ranked.sort(key=lambda x:-x[0])
    if not ranked or ranked[0][0]==0:
        return None,"NO_LEXICAL_OVERLAP"
    if len(ranked)>1 and ranked[0][0]==ranked[1][0]:
        return None,"LEXICAL_TIE"
    return ranked[0][1],"LEXICAL_MATCH_ONLY"


def select(queries,defs,mode):
    if mode not in ("B0","B1"):
        raise ValueError("unregistered selection baseline")
    rows=[]
    for q in queries:
        if mode=="B0":
            hit,why=lexical_select(q,defs)
        else:
            hit,why=schema_select(q,defs)
        rows.append({
            "opaque_id":q["opaque_id"],
            "selection_status":"selected" if hit else "abstain",
            "mechanism_id":hit["mechanism_id"] if hit else "-",
            "domain":hit["domain"] if hit else "-",
            "reason":why,
            "source_family":hit["source_family"] if hit else "-",
            "native_node_id":"-"  # B0/B1 are explicitly NON-DATABASE arms
        })
    return rows


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--manifest",required=True)
    parser.add_argument("--queries",required=True)
    parser.add_argument("--mode",required=True,choices=["B0","B1"])
    parser.add_argument("--out",required=True)
    a=parser.parse_args()
    defs=read(a.manifest,MANIFEST_FIELDS)
    tasks=read(a.queries,QUERY_FIELDS)
    out=select(tasks,defs,a.mode)
    write_tsv(a.out,ROUTE_FIELDS,out)
    print(f"f7_{a.mode}_routing=PASS total={len(out)} "
          f"selected={sum(x['selection_status']=='selected' for x in out)} "
          "model_used=NO")

if __name__=="__main__":
    main()
