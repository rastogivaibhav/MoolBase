#!/usr/bin/env python3
"""Adversarial Foundation v3 benchmark-instrument regression suite."""
import copy
import json
import math
import tempfile
import unittest
from pathlib import Path

from challenge import DOMAINS, SEED, cards_from, generate, sha, validate_packet
from scorer import AttemptError, grade, oracle, response_packet

HERE = Path(__file__).resolve().parent


class AppliedInstrumentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cards = cards_from(HERE / "cards.json")
        cls.packet = generate(cls.cards)

    def test_72_cases_and_boundary_counts(self):
        self.assertEqual(len(self.packet["tasks"]), 72)
        for domain in DOMAINS:
            tasks = [x for x in self.packet["tasks"] if x["domain"] == domain]
            self.assertEqual(len(tasks), 24)
            self.assertEqual(sum(oracle(t) is None for t in tasks), 4)
            self.assertEqual(sum(oracle(t) is not None for t in tasks), 20)

    def test_no_gold_or_answers_in_challenge(self):
        for t in self.packet["tasks"]:
            self.assertEqual(set(t),
                             {"task_id", "domain", "card_id", "task_type",
                              "target", "inputs", "prompt"})
        self.assertTrue(validate_packet(self.packet, self.cards))

    def test_seed_reproduction_and_parameter_change(self):
        self.assertEqual(self.packet, generate(self.cards, SEED))
        self.assertNotEqual(sha(self.packet), sha(generate(self.cards, SEED + 1)))

    def test_authorship_rights_cannot_be_laundered(self):
        changed = copy.deepcopy(self.cards)
        changed["publisher_rights"] = "unrestricted_use"
        with tempfile.TemporaryDirectory() as d:
            dest = Path(d) / "cards.json"
            dest.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "rights"):
                cards_from(dest)

    def test_independent_hand_worked_examples(self):
        def task(domain, target, params):
            return {"domain": domain, "target": target, "inputs": params}
        network = dict(queue=5, arrivals=9, service=7, buffer=6)
        self.assertEqual(oracle(task("networks", "next_queue", network)), 6)
        self.assertEqual(oracle(task("networks", "drops", network)), 1)
        biology = dict(observed=7, setpoint=10, gain=2, limit=4, disturbance=-1)
        self.assertEqual(oracle(task("biology", "control", biology)), 4)
        self.assertEqual(oracle(task("biology", "next_state", biology)), 10)
        stock = dict(daily_demand=5, lead_days=3, safety=4, on_hand=13,
                     on_order=1, backorders=2, target=30)
        self.assertEqual(oracle(task("supply_chain", "reorder_point", stock)), 19)
        self.assertEqual(oracle(task("supply_chain", "order_qty", stock)), 18)

    def test_oracle_selftest_not_model_comprehension(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        r = grade(self.packet, self.cards, s)
        self.assertEqual(r["metrics"]["correct_total"], 72)
        self.assertEqual(r["metrics"]["boundary_n"], 12)
        self.assertEqual(r["claim_status"], "EVALUATOR_SELFTEST_NOT_MODEL_LEARNING")
        self.assertFalse(r["model_executed"])

    def test_abstain_only_does_not_pass(self):
        s = response_packet(self.packet, "abstain_only_not_learned")
        r = grade(self.packet, self.cards, s)
        self.assertEqual(r["metrics"]["correct_total"], 12)
        self.assertEqual(r["metrics"]["application_accuracy"], 0)
        self.assertEqual(r["metrics"]["total_accuracy"], 1/6)

    def test_wrong_numeric_does_not_pass(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        row = next(x for x in s["responses"] if x["answer_type"] == "numeric")
        row["value"] += 100
        r = grade(self.packet, self.cards, s)
        self.assertEqual(r["metrics"]["correct_total"], 71)
        self.assertGreater(r["metrics"]["unsupported_numeric_rate"], 0)

    def test_wrong_citation_is_not_correct(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["responses"][0]["evidence_card_id"] = "madeup"
        self.assertEqual(grade(self.packet, self.cards, s)["metrics"]["correct_total"], 71)

    def test_numeric_hallucination_when_boundary_should_abstain(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        index = next(i for i, x in enumerate(self.packet["tasks"]) if oracle(x) is None)
        s["responses"][index].update(answer_type="numeric", value=42)
        self.assertEqual(grade(self.packet, self.cards, s)["metrics"]["correct_total"], 71)

    def test_tampered_challenge_hash_is_rejected(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        altered = copy.deepcopy(self.packet)
        altered["tasks"][0]["inputs"]["tampered"] = 44
        with self.assertRaisesRegex(AttemptError, "challenge mismatch"):
            grade(altered, self.cards, s)

    def test_gold_leak_in_question_packet_rejected(self):
        p = copy.deepcopy(self.packet)
        p["tasks"][0]["expected"] = 3
        with self.assertRaisesRegex(ValueError, "unapproved"):
            validate_packet(p, self.cards)

    def test_card_hash_mismatch_rejected(self):
        p = copy.deepcopy(self.packet)
        p["card_content_sha256"] = "0"*64
        with self.assertRaisesRegex(ValueError, "hash"):
            validate_packet(p, self.cards)

    def test_duplicate_and_missing_responses_rejected(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["responses"][1]["task_id"] = s["responses"][0]["task_id"]
        with self.assertRaisesRegex(AttemptError, "duplicate"):
            grade(self.packet, self.cards, s)
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["responses"].pop()
        with self.assertRaisesRegex(AttemptError, "missing or excess"):
            grade(self.packet, self.cards, s)

    def test_nonfinite_result_and_confidence_rejected(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        row = next(x for x in s["responses"] if x["answer_type"] == "numeric")
        row["value"] = math.nan
        with self.assertRaisesRegex(AttemptError, "finite"):
            grade(self.packet, self.cards, s)
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["responses"][0]["confidence"] = 9
        with self.assertRaisesRegex(AttemptError, "confidence"):
            grade(self.packet, self.cards, s)

    def test_abstention_requires_explanation(self):
        s = response_packet(self.packet, "abstain_only_not_learned")
        s["responses"][0]["reason"] = "?"
        with self.assertRaisesRegex(AttemptError, "rationale"):
            grade(self.packet, self.cards, s)

    def test_assumptions_acknowledged_required(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["responses"][0].pop("assumptions_acknowledged")
        with self.assertRaisesRegex(AttemptError, "acknowledgment"):
            grade(self.packet, self.cards, s)

    def test_scripted_oracle_cannot_pose_as_executed_model(self):
        s = response_packet(self.packet, "reference_oracle_not_learned")
        s["model_executed"] = True
        with self.assertRaisesRegex(AttemptError, "rebrand"):
            grade(self.packet, self.cards, s)

    def test_per_domain_tally(self):
        r = grade(self.packet, self.cards,
                  response_packet(self.packet, "reference_oracle_not_learned"))
        for v in r["per_domain"].values():
            self.assertEqual((v["n"], v["valid_application_n"], v["boundary_n"]),
                             (24, 20, 4))


if __name__ == "__main__":
    unittest.main(verbosity=2)
