#!/usr/bin/env python3
"""Fail-closed F5 manifest export and native receipt validation.

This exporter supplies an actual C++ MoolBase source adapter with project-
authored learning cards. It NEVER labels them Observed or Verified.
"""
import argparse
import csv
import json
import hashlib
from pathlib import Path
import sys

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
from challenge import cards_from

FIELDS=["card_id","domain","title","description","assumptions","boundaries"]
OUTPUT=["card_id","domain","title","description","assumptions","boundaries",
        "source_family","evidence_status","dwm_status","dwm_evidence_count",
        "dwm_primary_node","dwm_world_writes"]


def manifest(cards,path):
    records=[]
    for card in sorted(cards["cards"],key=lambda c:c["domain"]):
        item={"card_id":card["id"],"domain":card["domain"],
              "title":card["title"],"description":card["description"],
              "assumptions":"; ".join(card["assumptions"]),
              "boundaries":"; ".join(card["must_abstain_when"])}
        if not all(item.values()) or any("\t" in v or "\n" in v for v in item.values()):
            raise ValueError("unsafe or incomplete native manifest")
        records.append(item)
    with Path(path).open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=FIELDS,delimiter="\t",lineterminator="\n")
        w.writeheader()
        w.writerows(records)
    return records


def check_native(cards,receipts):
    if len(receipts)!=3:
        raise ValueError("native engine must return exactly 3 domain cards")
    originals={card["domain"]:card for card in cards["cards"]}
    by_domain={}
    for record in receipts:
        domain=record["domain"]
        if domain not in originals or domain in by_domain:
            raise ValueError("native result has unknown or duplicated domain")
        original=originals[domain]
        expected={
            "card_id":original["id"],"domain":domain,
            "title":original["title"],"description":original["description"],
            "assumptions":"; ".join(original["assumptions"]),
            "boundaries":"; ".join(original["must_abstain_when"]),
            "source_family":"original-project-authored-"+domain,
            "evidence_status":"unverified",
            "dwm_world_writes":"0"
        }
        if any(record.get(k)!=v for k,v in expected.items()):
            raise ValueError("native retrieval/source status mismatch for "+domain)
        try:
            count=int(record["dwm_evidence_count"])
            primary=int(record["dwm_primary_node"])
        except (ValueError,TypeError):
            raise ValueError("native reasoner receipt malformed")
        if count<0 or primary<0 or not record.get("dwm_status"):
            raise ValueError("native reasoner did not provide structured trace")
        by_domain[domain]=record
    return by_domain


def read_native(cards,path):
    with Path(path).open(encoding="utf-8",newline="") as f:
        r=csv.DictReader(f,delimiter="\t")
        if r.fieldnames!=OUTPUT:
            raise ValueError("native output contract differs from preregistration")
        return check_native(cards,list(r))


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--manifest")
    p.add_argument("--native")
    p.add_argument("--output")
    a=p.parse_args()
    cards=cards_from(a.cards)
    if bool(a.manifest)==bool(a.native):
        p.error("provide either --manifest or --native")
    if a.manifest:
        manifest(cards,a.manifest)
        print("f5_native_manifest=PASS records=3")
    else:
        result=read_native(cards,a.native)
        if a.output:
            Path(a.output).write_text(json.dumps(
                {"validated_native_receipts":result,
                 "source_classification":"project_authored_pedagogical",
                 "verified_evidence":0,
                 "modelworld_writes":0},indent=2,sort_keys=True)+"\n")
        print("f5_actual_MoolBase_and_HypoKosh_DWM=PASS cards=3 verified_evidence=0")


if __name__=="__main__":
    main()
