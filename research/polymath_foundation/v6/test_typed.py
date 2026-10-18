#!/usr/bin/env python3
"""F6 source/typed-contract negative tests. Optionally invoke actual C++ runner."""
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
from challenge import cards_from
from export_typed import (
    SEED,mechanisms,produce,write_tsv,MANIFEST_FIELDS,TASK_FIELDS,
    RECEIPT_FIELDS
)
from score_native import evaluate,load_receipts
from scorer import oracle

CARDS=HERE.parent/"v3"/"cards.json"
MECHANISMS=HERE/"mechanisms.json"

class F6ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards=cards_from(CARDS)
        cls.mech=mechanisms(MECHANISMS,cls.cards)
        cls.questions,cls.manifest,cls.rows=produce(cls.cards,cls.mech)

    def test_golden_fixed_sample(self):
        self.assertEqual(len(self.rows),72)
        for domain in ("networks","biology","supply_chain"):
            tasks=[t for t in self.questions["tasks"] if t["domain"]==domain]
            self.assertEqual(len(tasks),24)
            self.assertEqual(sum(oracle(t) is None for t in tasks),4)
            self.assertEqual(sum(oracle(t) is not None for t in tasks),20)

    def test_no_oracle_or_answer_in_export(self):
        for row in self.rows:
            self.assertEqual(set(row),set(TASK_FIELDS))
            for forbidden in ("expected","oracle","answer","gold"):
                self.assertNotIn(forbidden,row)
        self.assertNotIn("gold",json.dumps(self.rows).lower())

    def test_unit_typed_manifest(self):
        for row in self.manifest:
            self.assertEqual(row["source_status"],"unverified")
            self.assertEqual(row["license_status"],"project_authored")
            self.assertEqual(len(row["card_sha256"]),64)
            self.assertIn(":integer:",row["input_spec"]) if row["domain"]!="biology" else self.assertIn(":float:",row["input_spec"])

    def test_changed_source_status_denied(self):
        d=json.loads(MECHANISMS.read_text())
        d["mechanisms"][0]["source_status"]="Verified"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json"
            p.write_text(json.dumps(d))
            with self.assertRaisesRegex(ValueError,"provenance"):
                mechanisms(p,self.cards)

    def test_changed_license_denied(self):
        d=json.loads(MECHANISMS.read_text())
        d["mechanisms"][0]["license_status"]="permissive_external_training"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json"
            p.write_text(json.dumps(d))
            with self.assertRaisesRegex(ValueError,"provenance"):
                mechanisms(p,self.cards)

    def test_unknown_mechanism_domain_denied(self):
        d=json.loads(MECHANISMS.read_text())
        d["mechanisms"][0]["domain"]="unknown_domain"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json"
            p.write_text(json.dumps(d))
            with self.assertRaisesRegex(ValueError,"unknown procedure"):
                mechanisms(p,self.cards)

    def test_wrong_output_unit_contract_denied_at_export(self):
        d=json.loads(MECHANISMS.read_text())
        d["mechanisms"][0]["output_fields"][0][2]="unknown_unit"
        # Export itself can represent declared units, native allowlist must
        # independently reject altered field declarations.
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json"
            p.write_text(json.dumps(d))
            parsed=mechanisms(p,self.cards)
            _,new_manifest,_=produce(self.cards,parsed)
            self.assertNotEqual(new_manifest[0]["output_spec"],
                                self.manifest[0]["output_spec"])

    def test_duplicate_source_family_domain_denied(self):
        d=json.loads(MECHANISMS.read_text())
        d["mechanisms"][1]["source_family"]=d["mechanisms"][0]["source_family"]
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json"
            p.write_text(json.dumps(d))
            with self.assertRaisesRegex(ValueError,"provenance"):
                mechanisms(p,self.cards)

    def test_missing_required_input_not_repaired(self):
        record=next(r for r in self.rows if r["domain"]=="supply_chain")
        self.assertIn("=",record["inputs"])
        valid_field_count=len(self.mech["supply_chain"]["input_fields"])
        self.assertLessEqual(len(record["inputs"].split(";")),valid_field_count)

def execute(binary,manifest,rows):
    with tempfile.TemporaryDirectory() as root:
        m=Path(root)/"mechanisms.tsv"
        t=Path(root)/"tasks.tsv"
        o=Path(root)/"out.tsv"
        write_tsv(m,MANIFEST_FIELDS,manifest)
        write_tsv(t,TASK_FIELDS,rows)
        proc=subprocess.run([str(binary),str(m),str(t),str(o)],
                            capture_output=True,text=True,timeout=120)
        result=load_receipts(o) if proc.returncode==0 else None
        return proc,result

def native_checks(binary):
    cards=cards_from(CARDS)
    bydomain=mechanisms(MECHANISMS,cards)
    questions,manifest,tasks=produce(cards,bydomain)
    executed,rows=execute(binary,manifest,tasks)
    if executed.returncode:
        raise AssertionError("native executor failed: "+executed.stderr)
    scored=evaluate(cards,bydomain,rows)
    if (scored["metrics"]["n"]!=72 or
        scored["metrics"]["correct"]!=72 or
        scored["metrics"]["boundary_cases"]!=12):
        raise AssertionError("C++ scorer mismatch")

    def must_abstain(mutate,reason):
        changed=copy.deepcopy(tasks)
        idx=next(i for i,r in enumerate(changed) if r["domain"]==mutate[0])
        t=changed[idx]
        mutate[1](t)
        p,r=execute(binary,manifest,changed)
        if p.returncode:
            raise AssertionError("native execution unexpectedly crashed: "+p.stderr)
        item=r[idx]
        if item["status"]!="abstain" or item["reason"]!=reason:
            raise AssertionError(f"expected {reason}; got {item}")
        if item["value"]!="-":
            raise AssertionError("refusal emitted unsupported numeric answer")

    must_abstain(("networks",lambda t:t.__setitem__(
        "inputs",t["inputs"].replace("@packets#integer","@dollars#integer",1))),
                 "input_unit_mismatch")
    must_abstain(("networks",lambda t:t.__setitem__(
        "target_unit","dollars")), "target_unit_mismatch")
    must_abstain(("networks",lambda t:t.__setitem__(
        "mechanism_id","unknown_mechanism")), "unknown_mechanism")
    must_abstain(("biology",lambda t:t.__setitem__(
        "inputs",t["inputs"].replace("gain=", "gain=nan_invalid_",1))),
                 "invalid_or_out_of_range_numeric")
    must_abstain(("supply_chain",lambda t:t.__setitem__(
        "inputs",";".join(e for e in t["inputs"].split(";")
                        if not e.startswith("lead_days=")))),
                 "missing_required_input")

    # Enforce overflow guards on legal-shaped numeric input before arithmetic.
    def overflow(t):
        t["inputs"]=(
            "queue=9223372036854775807@packets#integer;"
            "arrivals=4@packets#integer;"
            "service=0@packets#integer;"
            "buffer=9223372036854775807@packets#integer")
    must_abstain(("networks",overflow),"integer_overflow")

    # Native source poisoning must fail before a receipt can be produced.
    for variant in ("source_status","input_spec","mechanism_id","card_sha256"):
        bad=copy.deepcopy(manifest)
        bad[0][variant]={"source_status":"verified",
                        "input_spec":"queue:integer: dollars",
                        "mechanism_id":"arbitrary-code",
                        "card_sha256":"not-a-real-hash"}[variant]
        p,_=execute(binary,bad,tasks)
        if p.returncode==0:
            raise AssertionError("native accepted poisoned manifest: "+variant)
    bad=copy.deepcopy(manifest)
    bad.append(copy.deepcopy(manifest[0]))
    p,_=execute(binary,bad,tasks)
    if p.returncode==0:
        raise AssertionError("native accepted repeated domain/ontology mapping")
    print("f6_native_adversarial_gates=PASS nonnumeric_abstention=6 poisoned_manifests=5",flush=True)

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--native-bin")
    args=parser.parse_args()
    tests=unittest.main(argv=["f6"],verbosity=2,exit=False)
    if not tests.result.wasSuccessful():
        raise SystemExit(1)
    if args.native_bin:
        native_checks(Path(args.native_bin).resolve())
