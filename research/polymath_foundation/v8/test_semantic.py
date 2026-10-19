#!/usr/bin/env python3
"""F8 isolated contracts; do not test-model or access external gold at inference."""
import json,sys,unittest,copy
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from challenge import cards_from
from export_typed import mechanisms
from generate_pilot import generate_pilot
from parse_semantics import rules,adapt,norm
from score_semantic import semantic_match,evaluate
from select_baselines import schema_select
from build_trials import records

class Contracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards=cards_from(HERE.parent/"v3"/"cards.json")
        cls.defs=mechanisms(HERE.parent/"v6"/"mechanisms.json",cls.cards)
        cls.questions,cls.gold,cls.source=generate_pilot(cls.cards,cls.defs)
        cls.registry=records(cls.cards,cls.defs)

    def test_count_classes(self):
        expected={"in_library_valid":12,"assumption_violation":6,
                  "unknown_mechanism":3,"composite":3}
        self.assertEqual(len(self.questions),24)
        self.assertEqual({k:sum(g["category"]==k for g in self.gold)
                          for k in expected},expected)

    def test_no_gold_in_candidate_question_schema(self):
        self.assertEqual(set(self.questions[0]),{"opaque_id","question"})
        for q in self.questions:
            self.assertFalse(any(k in q for k in ("domain","mechanism_id","target","inputs","answer")))

    def test_repeated_seed_stable(self):
        self.assertEqual((self.questions,self.gold,self.source),
                         generate_pilot(self.cards,self.defs))

    def test_positive_alias_baseline_resolves_without_gold(self):
        for q,g in zip(self.questions,self.gold):
            guess=rules(q["question"],q["opaque_id"])
            if g["category"] in ("in_library_valid","assumption_violation"):
                self.assertEqual(guess["status"],"normalize")
                self.assertTrue(semantic_match(guess["normalized"],g["expected_normalized"]))
                hit,_=schema_select({"target":guess["normalized"]["target"],
                     "target_unit":guess["normalized"]["target_unit"],
                     "inputs":guess["normalized"]["inputs"]},self.registry)
                self.assertIsNotNone(hit)
            else:
                self.assertEqual(guess["status"],"abstain")
                self.assertIsNone(guess["parser_error"])

    def test_bad_model_json_is_not_successful_abstention(self):
        bad=adapt("abc","this isn't JSON")
        self.assertEqual(bad["status"],"abstain")
        self.assertTrue(bad["parser_error"])

    def test_out_of_scope_model_abstention_requires_reason(self):
        x=adapt("xyz",'{"status":"abstain","reason":"no"}')
        self.assertTrue(x["parser_error"])
        x=adapt("xyz",'{"status":"abstain","reason":"Outside supported mechanisms."}')
        self.assertIsNone(x["parser_error"])

    def test_bad_duplicate_input_is_not_correct(self):
        bad=norm("id","next_queue","packets",
             "queue=1@packets#integer;queue=2@packets#integer")
        self.assertEqual(bad["parser_error"],"MALFORMED_TYPED_FIELDS")

    def test_wrong_units_not_fixed_by_parser(self):
        g=next(g for g in self.gold if g["category"]=="in_library_valid")
        copy_pred=copy.deepcopy(g["expected_normalized"])
        copy_pred["target_unit"]="credits"
        self.assertFalse(semantic_match(copy_pred,g["expected_normalized"]))

    def test_composite_semantic_gate_fails_closed(self):
        for q,g in zip(self.questions,self.gold):
            if g["category"]=="composite":
                self.assertEqual(rules(q["question"],q["opaque_id"])["status"],"abstain")

if __name__=="__main__":
    res=unittest.main(verbosity=2,exit=False)
    if not res.result.wasSuccessful():raise SystemExit(1)
