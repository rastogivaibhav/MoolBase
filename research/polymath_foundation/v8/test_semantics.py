#!/usr/bin/env python3
"""F8 negative controls: authored-ontology boundaries, no evaluator leakage."""
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
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from challenge import cards_from
from export_typed import mechanisms,write_tsv
from build_trials import QUERY_FIELDS
from build_semantic import ALIAS_FIELDS,construct,load_aliases
from semantic_baseline import AUDIT_FIELDS,resolve,normalize

CARDS=HERE.parent/"v3"/"cards.json"
MECH=HERE.parent/"v6"/"mechanisms.json"
ALIASES=HERE/"aliases.json"

def contract():
    cards=cards_from(CARDS)
    bydomain=mechanisms(MECH,cards)
    aliases=load_aliases(ALIASES,bydomain)
    queries,gold=construct(cards,bydomain,aliases)
    return cards,bydomain,aliases,queries,gold

class V8Contracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards,cls.by,cls.aliases,cls.queries,cls.gold=contract()
        cls.normalized,cls.audit=resolve(cls.queries,cls.aliases)

    def test_trial_counts_and_refusal_kinds(self):
        self.assertEqual(len(self.queries),120)
        for cat,n in [("library",72),("unsupported_unit",12),
                      ("conflicting_alias",12),("out_of_domain",12),
                      ("composite",12)]:
            self.assertEqual(sum(g["category"]==cat for g in self.gold),n)

    def test_unlabeled_only_no_gold(self):
        self.assertTrue(all(set(q)==set(QUERY_FIELDS) for q in self.queries))
        self.assertTrue(all("mechanism_id" not in q and "domain" not in q
                            for q in self.queries))
        self.assertEqual(len(set(q["opaque_id"] for q in self.queries)),120)

    def test_exact_seed_repeat(self):
        self.assertEqual((self.queries,self.gold),
                         construct(self.cards,self.by,self.aliases))

    def test_provenance_registry_not_empirically_verified(self):
        self.assertEqual(len(self.aliases),44)
        self.assertTrue(all(a["source_status"]=="unverified" for a in self.aliases.values()))

    def test_all_curated_known_trials_normalize(self):
        for q,g,audit in zip(self.queries,self.gold,self.audit):
            if g["category"]=="library":
                self.assertEqual(audit["status"],"normalized",q["opaque_id"])
            else:
                self.assertEqual(audit["status"],"abstain",q["opaque_id"])

    def test_units_no_silent_conversion(self):
        q=next(q for q,g in zip(self.queries,self.gold)
               if g["category"]=="unsupported_unit")
        result,why,_=normalize(q,self.aliases)
        self.assertIsNone(result)
        self.assertEqual(why,"INPUT_UNIT_MISMATCH")

    def test_duplicate_concept_alias_rejected(self):
        q=next(q for q,g in zip(self.queries,self.gold)
               if g["category"]=="conflicting_alias")
        result,why,_=normalize(q,self.aliases)
        self.assertIsNone(result)
        self.assertEqual(why,"DUPLICATE_CANONICAL_CONCEPT")

    def test_cross_domain_composite_denied(self):
        q=next(q for q,g in zip(self.queries,self.gold)
               if g["category"]=="composite")
        result,why,_=normalize(q,self.aliases)
        self.assertIsNone(result)
        self.assertEqual(why,"CROSS_DOMAIN_OR_PROVENANCE")

    def test_changed_source_status_denied(self):
        doc=json.loads(ALIASES.read_text())
        doc["entries"][0]["source_status"]="Verified"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json";p.write_text(json.dumps(doc))
            with self.assertRaisesRegex(ValueError,"source alias promoted"):
                load_aliases(p,self.by)

    def test_duplicate_alias_rejected(self):
        doc=json.loads(ALIASES.read_text())
        doc["entries"].append(copy.deepcopy(doc["entries"][0]))
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json";p.write_text(json.dumps(doc))
            with self.assertRaisesRegex(ValueError,"duplicate"):
                load_aliases(p,self.by)

    def test_unapproved_unit_conversion_rejected(self):
        doc=json.loads(ALIASES.read_text())
        doc["entries"][0]["alias_unit"]="fictional_new_unit"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json";p.write_text(json.dumps(doc))
            with self.assertRaisesRegex(ValueError,"conversion"):
                load_aliases(p,self.by)

    def test_wrong_concept_type_cannot_pass(self):
        doc=json.loads(ALIASES.read_text())
        doc["entries"][0]["canonical_type"]="float" if doc["entries"][0]["canonical_type"]=="integer" else "integer"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"bad.json";p.write_text(json.dumps(doc))
            with self.assertRaisesRegex(ValueError,"type/units"):
                load_aliases(p,self.by)

def native_validate(binary):
    cards,bydomain,aliases,queries,gold=contract()
    expected,expected_audit=resolve(queries,aliases)
    with tempfile.TemporaryDirectory() as d:
        root=Path(d)
        registry=root/"aliases.tsv"
        inp=root/"questions.tsv"
        out=root/"normalized.tsv"
        audit=root/"audit.tsv"
        source=[{k:str(v[k]) for k in ALIAS_FIELDS}
                for v in sorted(aliases.values(),key=lambda x:x["alias"])]
        write_tsv(registry,ALIAS_FIELDS,source)
        write_tsv(inp,QUERY_FIELDS,queries)
        def call():
            return subprocess.run([str(binary),str(registry),str(inp),
                                   str(out),str(audit)],capture_output=True,
                                  text=True,timeout=100)
        process=call()
        if process.returncode:
            raise RuntimeError("native v8 failed: "+process.stderr)
        with out.open(newline="",encoding="utf-8") as f:
            nd=csv.DictReader(f,delimiter="\t")
            if nd.fieldnames!=list(QUERY_FIELDS):
                raise RuntimeError("native output header changed")
            native=list(nd)
        with audit.open(newline="",encoding="utf-8") as f:
            au=csv.DictReader(f,delimiter="\t")
            if au.fieldnames!=list(AUDIT_FIELDS):
                raise RuntimeError("native audit header changed")
            native_audit=list(au)
        if native!=expected:
            bad=next(((x,y) for x,y in zip(native,expected) if x!=y),None)
            raise RuntimeError("native vs nonnative canonical query mismatch: "+str(bad))
        for a,b in zip(native_audit,expected_audit):
            for key in ("opaque_id","status","reason","alias_count","source_status"):
                if a[key]!=b[key]:
                    raise RuntimeError("B1/B2 semantic audit divergence "+key+
                                       " "+a["opaque_id"]+": "+a[key]+" vs "+b[key])
            if a["status"]=="normalized" and (a["node_receipts"]=="-" or not a["node_receipts"]):
                raise RuntimeError("native MoolBase did not provide node IDs")
        for kind in ("source_status","relation_kind","source_family","alias"):
            bad=copy.deepcopy(source)
            bad[0][kind]={
                "source_status":"Verified","relation_kind":"causal_truth",
                "source_family":"faked-independent-evidence",
                "alias":source[1]["alias"]
            }[kind]
            write_tsv(registry,ALIAS_FIELDS,bad)
            poisoned=call()
            if poisoned.returncode==0:
                raise RuntimeError("native accepted poisoned ontology "+kind)
        print("f8_real_native_semantic_e2e=PASS 120_cases "
              "B1_B2_byte_equivalence=YES 4_source_poison_controls=PASS")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--native-bin")
    a=p.parse_args()
    result=unittest.main(argv=["v8"],verbosity=2,exit=False)
    if not result.result.wasSuccessful():
        raise SystemExit(1)
    if a.native_bin:
        native_validate(Path(a.native_bin).resolve())
