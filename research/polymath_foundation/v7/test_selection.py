#!/usr/bin/env python3
"""F7 honest trial/selection contract tests and actual native integration guards."""
import argparse
import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v6"))
from challenge import cards_from
from export_typed import mechanisms,write_tsv,MANIFEST_FIELDS
from build_trials import (build,records,QUERY_FIELDS,ROUTE_FIELDS)
from select_baselines import schema_select,select,decode_inputs
from route_and_score import prepare

CARDS=HERE.parent/"v3"/"cards.json"
MECH=HERE.parent/"v6"/"mechanisms.json"

class F7Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards=cards_from(CARDS)
        cls.defs=mechanisms(MECH,cls.cards)
        cls.original,cls.questions,cls.gold=build(cls.cards,cls.defs)
        cls.manifest=records(cls.cards,cls.defs)

    def test_fixed_distribution(self):
        self.assertEqual(len(self.questions),108)
        expected={"library":72,"poison_unit":12,"out_of_domain":12,
                  "cross_domain_composite":12}
        self.assertEqual({key:sum(g["category"]==key for g in self.gold)
                          for key in expected},expected)

    def test_no_domain_or_mechanism_leak_in_queries(self):
        for q in self.questions:
            self.assertEqual(set(q),set(QUERY_FIELDS))
            self.assertTrue(q["opaque_id"].startswith("trial-"))
            self.assertNotIn("mechanism_id",q)
            self.assertNotIn("domain",q)

    def test_fixed_random_shuffle(self):
        self.assertEqual((self.questions,self.gold),
                         build(self.cards,self.defs)[1:])

    def test_schema_only_routes_72_and_blocks_36(self):
        outcomes=select(self.questions,self.manifest,"B1")
        byid=dict(zip((q["opaque_id"] for q in self.questions),
                      self.gold))
        for r in outcomes:
            g=byid[r["opaque_id"]]
            if g["category"]=="library":
                self.assertEqual(r["selection_status"],"selected")
                self.assertEqual(r["domain"],g["expected_domain"])
            else:
                self.assertEqual(r["selection_status"],"abstain")

    def test_lexical_baseline_is_non_native(self):
        outcomes=select(self.questions,self.manifest,"B0")
        self.assertEqual(len(outcomes),108)
        self.assertTrue(all(r["native_node_id"]=="-" for r in outcomes))

    def test_bad_type_unit_declined(self):
        q=next(q for q,g in zip(self.questions,self.gold)
               if g["category"]=="poison_unit")
        choice,_=schema_select(q,self.manifest)
        self.assertIsNone(choice)

    def test_out_of_domain_is_declined(self):
        for q,g in zip(self.questions,self.gold):
            if g["category"]=="out_of_domain":
                choice,_=schema_select(q,self.manifest)
                self.assertIsNone(choice)

    def test_composite_not_allowed_to_partially_match(self):
        for q,g in zip(self.questions,self.gold):
            if g["category"]=="cross_domain_composite":
                choice,_=schema_select(q,self.manifest)
                self.assertIsNone(choice)

    def test_malformed_or_duplicate_parameter_rejected(self):
        self.assertIsNone(decode_inputs("queue=2@packets#integer;queue=3@packets#integer"))
        self.assertIsNone(decode_inputs("queue:2@packets#integer"))

    def test_routing_rejects_duplicate_question_ids(self):
        routes=select(self.questions,self.manifest,"B1")
        bad=copy.deepcopy(routes)
        bad[1]["opaque_id"]=bad[0]["opaque_id"]
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):
                prepare(self.questions,bad,Path(root)/"selected.tsv")


def end_to_end(native):
    cards=cards_from(CARDS)
    definitions=mechanisms(MECH,cards)
    _,queries,gold=build(cards,definitions)
    manifest=records(cards,definitions)
    baseline=select(queries,manifest,"B1")
    with tempfile.TemporaryDirectory() as root:
        root=Path(root)
        m=root/"mechanisms.tsv"
        q=root/"queries.tsv"
        r=root/"native.tsv"
        write_tsv(m,MANIFEST_FIELDS,manifest)
        write_tsv(q,QUERY_FIELDS,queries)
        proc=subprocess.run([str(native),str(m),str(q),str(r)],
                            text=True,capture_output=True,timeout=60)
        if proc.returncode!=0:
            raise RuntimeError("native selector failed: "+proc.stderr)
        import csv
        with r.open(newline="",encoding="utf-8") as f:
            rd=csv.DictReader(f,delimiter="\t")
            if rd.fieldnames!=list(ROUTE_FIELDS):
                raise RuntimeError("native receipt header corrupt")
            rows=list(rd)
        if len(rows)!=108:
            raise RuntimeError("native selector ignored frozen 108 cases")
        for nonnative,actual in zip(baseline,rows):
            if actual["opaque_id"]!=nonnative["opaque_id"]:
                raise RuntimeError("native query order changed")
            for key in ("selection_status","mechanism_id","domain","reason",
                        "source_family"):
                if nonnative[key]!=actual[key]:
                    raise RuntimeError("B1 vs B2 divergence in "+key+
                                       " on "+actual["opaque_id"])
            if actual["selection_status"]=="selected" and actual["native_node_id"]=="-":
                raise RuntimeError("native candidate missing real DB node ID")
        for kind in ("source_status","source_family","input_spec",
                     "mechanism_id","card_sha256"):
            poisoned=copy.deepcopy(manifest)
            poisoned[0][kind]={
                "source_status":"Verified",
                "source_family":"fabricated-independent-copy",
                "input_spec":"queue:float:credits",
                "mechanism_id":"unreviewed_exec_v9",
                "card_sha256":"bad_hash"}[kind]
            write_tsv(m,MANIFEST_FIELDS,poisoned)
            p=subprocess.run([str(native),str(m),str(q),str(r)],
                             text=True,capture_output=True,timeout=30)
            if p.returncode==0:
                raise RuntimeError("native source poisoning accepted "+kind)
        poisoned=copy.deepcopy(manifest)
        poisoned.append(poisoned[0])
        write_tsv(m,MANIFEST_FIELDS,poisoned)
        p=subprocess.run([str(native),str(m),str(q),str(r)],
                         text=True,capture_output=True,timeout=30)
        if p.returncode==0:
            raise RuntimeError("native duplicate source accepted")
        print("f7_native_selection_e2e=PASS tasks=108 B1_B2_equivalent=YES "
              "poisoned_registry_denied=6 verified_facts=0",flush=True)

if __name__=="__main__":
    arg=argparse.ArgumentParser()
    arg.add_argument("--native")
    a=arg.parse_args()
    result=unittest.main(argv=["f7"],verbosity=2,exit=False)
    if not result.result.wasSuccessful():
        raise SystemExit(1)
    if a.native:
        end_to_end(Path(a.native).resolve())
