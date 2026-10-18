#!/usr/bin/env python3
"""Pure contract tests for v4; deliberately execute NO model."""
import json
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "v3"))
from challenge import cards_from, generate, sha
from run_actual_model import choose_tasks, build_prompt, adapt, reject
from grade_pilot import metrics


class F4ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards = cards_from(Path(__file__).resolve().parents[1] /
                               "v3" / "cards.json")
        cls.master, cls.tasks = choose_tasks(cls.cards)
        cls.card_map = {c["id"]: c for c in cls.cards["cards"]}

    def test_fixed_per_domain_selection(self):
        self.assertEqual(len(self.master["tasks"]), 72)
        self.assertEqual(len(self.tasks), 24)
        self.assertEqual({d: sum(t["domain"] == d for t in self.tasks)
                          for d in ("networks", "biology", "supply_chain")},
                         {"networks": 8, "biology": 8, "supply_chain": 8})

    def test_selection_reproducible(self):
        master, tasks = choose_tasks(self.cards)
        self.assertEqual(sha(self.master), sha(master))
        self.assertEqual(tasks, self.tasks)

    def test_no_gold_in_model_prompts(self):
        for task in self.tasks:
            for arm in ("A0", "A1"):
                prompt = build_prompt(task,self.card_map[task["card_id"]],arm)
                self.assertIn(task["target"],prompt)
                self.assertIn(json.dumps(task["inputs"],sort_keys=True),prompt)
                self.assertNotIn("gold_answer",prompt)
                self.assertNotIn("expected_output",prompt)

    def test_closed_book_does_not_receive_equations_from_card(self):
        for task in self.tasks:
            card=self.card_map[task["card_id"]]
            a0=build_prompt(task,card,"A0")
            a1=build_prompt(task,card,"A1")
            self.assertNotIn("INSTRUCTIONAL CARD:",a0)
            self.assertIn("INSTRUCTIONAL CARD:",a1)
            self.assertIn(card["description"],a1)

    def test_correct_parse_preserves_values(self):
        task=self.tasks[0]
        obj={"answer_type":"numeric","value":17,
             "evidence_card_id":"deliberately-wrong",
             "assumptions_acknowledged":True,"confidence":0.25}
        converted,error=adapt(json.dumps(obj),task)
        self.assertIsNone(error)
        self.assertEqual(converted["value"],17)
        self.assertEqual(converted["evidence_card_id"],"deliberately-wrong")

    def test_bad_model_output_cannot_pass_boundary_by_abstaining(self):
        for task in self.tasks:
            converted,error=adapt("some unparseable prose",task)
            self.assertIsNotNone(error)
            self.assertFalse(converted["assumptions_acknowledged"])
            self.assertNotEqual(converted["evidence_card_id"],task["card_id"])

    def test_invalid_numeric_is_not_repaired(self):
        task=self.tasks[0]
        converted,error=adapt(json.dumps({
            "answer_type":"numeric","value":"42",
            "evidence_card_id":task["card_id"],
            "assumptions_acknowledged":True}),task)
        self.assertIsNotNone(error)
        self.assertEqual(converted["evidence_card_id"],
                         "INVALID_MODEL_OUTPUT_NOT_EVIDENCE")

    def test_out_of_range_confidence_invalidates_model_answer(self):
        task=self.tasks[0]
        _,error=adapt(json.dumps({
            "answer_type":"abstain","reason":"Model assumptions are violated.",
            "evidence_card_id":task["card_id"],
            "assumptions_acknowledged":True,"confidence":2}),task)
        self.assertIsNotNone(error)

    def test_correct_json_abstention_requires_reason(self):
        task=self.tasks[0]
        _,error=adapt(json.dumps({
            "answer_type":"abstain","reason":"invalid",
            "evidence_card_id":task["card_id"],
            "assumptions_acknowledged":True}),task)
        self.assertIsNotNone(error)

    def test_metric_bounds(self):
        records=[{"is_boundary_case":False,"correct":False,
                  "citation_valid":True,"wrong_numeric_or_unsupported":True,
                  "answered_numeric":True},
                 {"is_boundary_case":True,"correct":True,
                  "citation_valid":True,"wrong_numeric_or_unsupported":False,
                  "answered_numeric":False}]
        v=metrics(records)
        self.assertEqual(v["n"],2)
        self.assertEqual(v["correct_n"],1)
        self.assertEqual(v["boundary_abstention_rate"],1)
        self.assertEqual(v["valid_application_accuracy"],0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
