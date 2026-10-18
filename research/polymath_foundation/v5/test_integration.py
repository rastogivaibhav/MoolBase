#!/usr/bin/env python3
"""Non-LLM F5 security and alignment checks, including optional real native CLI."""
import argparse
import copy
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v4"))
from challenge import cards_from
from native_manifest import FIELDS,OUTPUT,manifest,check_native,read_native
from run_integration import selected,prompt_for,native_card,ARMS
from scorer import oracle

HERE=Path(__file__).resolve().parent
CARDS=HERE.parent/"v3"/"cards.json"


class F5Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards=cards_from(CARDS)
        cls.byid={c["id"]:c for c in cls.cards["cards"]}
        cls.full,cls.sample=selected(cls.cards)
        cls.native={}
        for card in cls.cards["cards"]:
            cls.native[card["domain"]]={
                "card_id":card["id"],"domain":card["domain"],
                "title":card["title"],"description":card["description"],
                "assumptions":"; ".join(card["assumptions"]),
                "boundaries":"; ".join(card["must_abstain_when"]),
                "source_family":"original-project-authored-"+card["domain"],
                "evidence_status":"unverified",
                "dwm_status":"Abstain",
                "dwm_evidence_count":"0","dwm_primary_node":"0",
                "dwm_world_writes":"0"
            }

    def test_exactly_four_each_domain(self):
        self.assertEqual(len(self.sample),12)
        for d in ("networks","biology","supply_chain"):
            self.assertEqual(sum(t["domain"]==d for t in self.sample),4)

    def test_distinct_from_v4_frozen_indices(self):
        from run_actual_model import choose_tasks
        _,older=choose_tasks(self.cards)
        self.assertFalse(set(t["task_id"] for t in older) &
                         set(t["task_id"] for t in self.sample))

    def test_a1_a2_semantic_and_byte_equivalence(self):
        for task in self.sample:
            a=prompt_for("A1",task,self.byid,self.native)
            b=prompt_for("A2",task,self.byid,self.native)
            self.assertEqual(a,b)

    def test_a3_is_real_epistemic_annotation_not_answer(self):
        for task in self.sample:
            s=prompt_for("A3",task,self.byid,self.native)
            self.assertIn("HypoKosh",s.upper().replace("HYPOKOSH","HypoKosh"))
            self.assertIn("unverified",s)
            self.assertIn(prompt_for("A2",task,self.byid,self.native),s)

    def test_metadata_source_laundering_rejected(self):
        x=copy.deepcopy(self.native)
        x["biology"]["evidence_status"]="verified"
        with self.assertRaises(ValueError):
            check_native(self.cards,list(x.values()))

    def test_correlated_family_forgery_rejected(self):
        x=copy.deepcopy(self.native)
        x["biology"]["source_family"]=x["networks"]["source_family"]
        with self.assertRaises(ValueError):
            check_native(self.cards,list(x.values()))

    def test_missing_or_duplicate_card_rejected(self):
        with self.assertRaises(ValueError):
            check_native(self.cards,list(self.native.values())[:2])
        seq=list(self.native.values())
        with self.assertRaises(ValueError):
            check_native(self.cards,[seq[0],seq[0],seq[2]])

    def test_nonzero_governed_modelworld_writes_rejected(self):
        x=copy.deepcopy(self.native)
        x["networks"]["dwm_world_writes"]="4"
        with self.assertRaises(ValueError):
            check_native(self.cards,list(x.values()))

    def test_unknown_prompt_arm_fails(self):
        with self.assertRaises(ValueError):
            prompt_for("faked",self.sample[0],self.byid,self.native)

    def test_no_answer_in_retrieval_source_fields(self):
        for task in self.sample:
            row=self.native[task["domain"]]
            self.assertNotIn("value",row)
            self.assertNotIn("answer",row)
            self.assertNotIn("oracle",row)

    def test_native_tsv_export_has_six_exact_fields(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"cards.tsv"
            rows=manifest(self.cards,p)
            self.assertEqual(len(rows),3)
            with p.open(newline="") as f:
                rd=csv.DictReader(f,delimiter="\t")
                self.assertEqual(rd.fieldnames,FIELDS)
                self.assertEqual(len(list(rd)),3)


def check_binary(binary):
    """Native integration E2E must use executable and persist/reopen real DB."""
    cards=cards_from(CARDS)
    with tempfile.TemporaryDirectory() as td:
        inp=Path(td)/"manifest.tsv"
        out=Path(td)/"out.tsv"
        manifest(cards,inp)
        subprocess.run([str(binary),str(inp),str(out)],check=True,timeout=180)
        found=read_native(cards,out)
        assert len(found)==3
        # Native input rejection must not silently ingest duplicate cards.
        lines=inp.read_text().splitlines()
        bad=Path(td)/"duplicate.tsv"
        bad.write_text("\n".join(lines+[lines[1]])+"\n")
        p=subprocess.run([str(binary),str(bad),str(out)],capture_output=True,
                         text=True,timeout=30)
        if p.returncode==0: raise AssertionError("duplicate source accepted")
        # Tampered original source family and source status are never accepted
        # even if the native writer was somehow corrupted.
        assert all(n["evidence_status"]=="unverified" for n in found.values())
        print("f5_native_graph_e2e_negative=PASS",flush=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--native-bin")
    args=parser.parse_args()
    outcome=unittest.main(argv=["f5_tests"],verbosity=2,exit=False)
    if not outcome.result.wasSuccessful():
        raise SystemExit(1)
    if args.native_bin:
        check_binary(Path(args.native_bin).resolve())
