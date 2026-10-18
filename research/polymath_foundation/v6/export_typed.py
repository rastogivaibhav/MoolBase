#!/usr/bin/env python3
"""Export frozen v6 typed input records and verify immutable mechanism contracts.

Does not calculate answers or disclose v3 oracle results to native MoolBase.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
from challenge import cards_from,generate,sha

SEED=20261019
MANIFEST_FIELDS=("mechanism_id","version","domain","card_id","formula_id",
                 "source_family","source_status","license_status","description",
                 "input_spec","output_spec","card_sha256")
TASK_FIELDS=("task_id","domain","mechanism_id","target","target_unit","inputs")
RECEIPT_FIELDS=("task_id","domain","mechanism_id","target","target_unit","status",
                "value","reason","source_family","source_status",
                "policy_version","native_node_id","scientific_truth_claim")

def mechanisms(path,cards):
    doc=json.loads(Path(path).read_text(encoding="utf-8"))
    if doc.get("schema_version")!="moolbase.polymath.typed.mechanism.v1":
        raise ValueError("unknown typed schema")
    if doc.get("classification")!="project_authored_pedagogical_not_scientifically_verified":
        raise ValueError("source authorship/trust mislabelled")
    records=doc.get("mechanisms")
    if not isinstance(records,list) or len(records)!=3:
        raise ValueError("must include three typed procedures")
    card_by_domain={c["domain"]:c for c in cards["cards"]}
    output={}
    for m in records:
        domain=m["domain"]
        if domain not in card_by_domain or domain in output:
            raise ValueError("duplicate or unknown procedure domain")
        if (m.get("card_id")!=card_by_domain[domain]["id"] or
            m.get("source_status")!="unverified" or
            m.get("license_status")!="project_authored" or
            m.get("scientific_truth_claim") is not False or
            m.get("source_family")!="original-project-authored-"+domain):
            raise ValueError("untrusted/unsupported mechanism provenance")
        if m.get("version")!="1" or not m["mechanism_id"].endswith(".v1"):
            raise ValueError("unapproved mechanism version")
        for section in ("input_fields","output_fields"):
            arr=m.get(section)
            if not isinstance(arr,list) or len(arr)<2:
                raise ValueError("missing type and unit information")
            names=[]
            for item in arr:
                if (not isinstance(item,list) or len(item)!=3 or
                    not all(isinstance(x,str) and x and
                            all(y.isalnum() or y=="_" for y in x) for x in item)
                    or item[1] not in ("integer","float")):
                    raise ValueError("invalid input/output descriptor")
                names.append(item[0])
            if len(names)!=len(set(names)):
                raise ValueError("repeated field")
        required=set(card_by_domain[domain]["inputs"])
        if set(row[0] for row in m["input_fields"])!=required:
            raise ValueError("mechanism inputs disagree with card")
        if set(row[0] for row in m["output_fields"])!=set(card_by_domain[domain]["outputs"]):
            raise ValueError("mechanism outputs disagree with card")
        output[domain]=m
    return output

def safe(s):
    if not isinstance(s,str) or not s or any(ch in s for ch in "\t\n\r;|="):
        raise ValueError("unsafe manifest field")
    return s

def spec(fields):
    return "|".join(":".join(safe(x) for x in row) for row in fields)

def write_tsv(path,header,rows):
    target=Path(path)
    target.parent.mkdir(parents=True,exist_ok=True)
    with target.open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=header,delimiter="\t",
                              lineterminator="\n",extrasaction="raise")
        writer.writeheader()
        writer.writerows(rows)

def produce(cards,bydomain):
    manifest=[]
    source_digest=sha(cards)
    for domain,m in sorted(bydomain.items()):
        manifest.append({
            "mechanism_id":safe(m["mechanism_id"]),"version":m["version"],
            "domain":domain,"card_id":m["card_id"],"formula_id":safe(m["formula_id"]),
            "source_family":m["source_family"],
            "source_status":m["source_status"],"license_status":m["license_status"],
            "description":safe(m["description"]),
            "input_spec":spec(m["input_fields"]),
            "output_spec":spec(m["output_fields"]),
            "card_sha256":source_digest
        })
    questions=generate(cards,SEED)
    if len(questions["tasks"])!=72:
        raise ValueError("invalid frozen question count")
    taskrows=[]
    for q in questions["tasks"]:
        m=bydomain[q["domain"]]
        typemap={f[0]:(f[1],f[2]) for f in m["input_fields"]}
        outmap={f[0]:(f[1],f[2]) for f in m["output_fields"]}
        entries=[]
        for name in [x[0] for x in m["input_fields"]]:
            if name not in q["inputs"]:
                continue  # boundary case: never fill missing data
            value=q["inputs"][name]
            ty,unit=typemap[name]
            if type(value) not in (int,float):
                raise ValueError("unexpected generator type")
            entries.append(f"{name}={value}@{unit}#{ty}")
        taskrows.append({
            "task_id":q["task_id"],"domain":q["domain"],
            "mechanism_id":m["mechanism_id"],"target":q["target"],
            "target_unit":outmap[q["target"]][1],
            "inputs":";".join(entries)
        })
    return questions,manifest,taskrows

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--mechanisms",required=True)
    p.add_argument("--manifest",required=True)
    p.add_argument("--tasks",required=True)
    p.add_argument("--receipt",required=True)
    args=p.parse_args()
    cards=cards_from(args.cards)
    bydomain=mechanisms(args.mechanisms,cards)
    qs,manifest,rows=produce(cards,bydomain)
    write_tsv(args.manifest,MANIFEST_FIELDS,manifest)
    write_tsv(args.tasks,TASK_FIELDS,rows)
    receipt={
        "experiment":"MOOL-POLYMATH-F6-001",
        "seed":SEED,
        "full_question_sha256":sha(qs),
        "cards_sha256":sha(cards),
        "mechanism_file_sha256":hashlib.sha256(Path(args.mechanisms).read_bytes()).hexdigest(),
        "tasks_tsv_sha256":hashlib.sha256(Path(args.tasks).read_bytes()).hexdigest(),
        "manifest_tsv_sha256":hashlib.sha256(Path(args.manifest).read_bytes()).hexdigest(),
        "question_count":len(rows),
        "source_status":"project_authored_unverified",
        "oracle_access":"NOT_EXPORTED"
    }
    Path(args.receipt).write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n",
                                  encoding="utf-8")
    print(f"f6_typed_input_export=PASS rows={len(rows)} gold_exported=0")


if __name__=="__main__":
    main()
