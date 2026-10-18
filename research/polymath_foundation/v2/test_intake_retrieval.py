#!/usr/bin/env python3
"""Adversarial metadata provenance and attribution retrieval tests (v2)."""
import copy
import json
import tempfile
import unittest
from pathlib import Path

from intake_retrieval import (
    IntakeError, evaluate, search, tfidf_index,
    validate_collection, validate_questions
)

HERE = Path(__file__).resolve().parent


def payload():
    return json.loads((HERE / "source_notes.json").read_text(encoding="utf-8"))


def questions():
    return json.loads((HERE / "evaluation_questions.json").read_text(encoding="utf-8"))


class FoundationV2Tests(unittest.TestCase):
    def test_actual_external_source_manifest(self):
        sources, notes = validate_collection(payload())
        self.assertEqual(len(sources), 7)
        self.assertEqual(len(notes), 12)
        self.assertTrue(all(x["content_form"] == "author_paraphrase"
                            for x in notes.values()))
        self.assertTrue(all(x["verification_status"] == "human_review_needed"
                            for x in notes.values()))

    def test_deterministic_evaluation_and_no_comprehension_claim(self):
        a = evaluate(payload(), questions())
        b = evaluate(payload(), questions())
        self.assertEqual(a, b)
        self.assertEqual(a["domain_comprehension"], "NOT_ASSESSED")
        self.assertEqual(a["source_authentication"], "NOT_ESTABLISHED")
        self.assertFalse(a["external_text_training"])
        self.assertEqual(a["question_count"], 16)
        self.assertEqual(set(a["methods"]), {"B0", "B1"})

    def test_unlicensed_source_cannot_assert_permissive_reuse(self):
        doc = payload()
        doc["sources"][0]["license_status"] = "permissive_train_everything"
        with self.assertRaisesRegex(IntakeError, "license"):
            validate_collection(doc)

    def test_publisher_text_cannot_masquerade_as_authored_note(self):
        doc = payload()
        doc["notes"][0]["content_form"] = "publisher_verbatim"
        with self.assertRaisesRegex(IntakeError, "mislabelled"):
            validate_collection(doc)

    def test_training_permission_cannot_be_invented(self):
        doc = payload()
        doc["notes"][0]["allow_model_training"] = True
        with self.assertRaisesRegex(IntakeError, "permission"):
            validate_collection(doc)

    def test_derived_note_cannot_be_declared_verified(self):
        doc = payload()
        doc["notes"][0]["verification_status"] = "verified"
        with self.assertRaisesRegex(IntakeError, "autonomously verified"):
            validate_collection(doc)
        doc = payload()
        doc["notes"][0]["verified"] = True
        with self.assertRaisesRegex(IntakeError, "status laundering"):
            validate_collection(doc)

    def test_duplicate_source_rejected(self):
        doc = payload()
        doc["sources"].append(copy.deepcopy(doc["sources"][0]))
        with self.assertRaisesRegex(IntakeError, "duplicate source"):
            validate_collection(doc)

    def test_dangling_evidence_source_rejected(self):
        doc = payload()
        doc["notes"][0]["source_id"] = "missing"
        with self.assertRaisesRegex(IntakeError, "dangling source"):
            validate_collection(doc)

    def test_ontology_domain_mismatch_rejected(self):
        doc = payload()
        doc["notes"][0]["domain"] = "biology"
        with self.assertRaisesRegex(IntakeError, "domain conflict"):
            validate_collection(doc)

    def test_missing_boundary_or_assumptions_rejected(self):
        doc = payload()
        doc["notes"][0]["boundaries"] = []
        with self.assertRaisesRegex(IntakeError, "boundaries"):
            validate_collection(doc)

    def test_invalid_citation_anchor_rejected(self):
        doc = payload()
        doc["notes"][0]["source_anchor"] = "invented unsupported section"
        with self.assertRaisesRegex(IntakeError, "citation anchor"):
            validate_collection(doc)

    def test_non_https_source_rejected(self):
        doc = payload()
        doc["sources"][0]["url"] = "file:///etc/passwd"
        with self.assertRaisesRegex(IntakeError, "HTTPS"):
            validate_collection(doc)

    def test_question_with_unregistered_target_rejected(self):
        doc = payload()
        _, notes = validate_collection(doc)
        q = questions()
        q["questions"][0]["gold_note_ids"] = ["invented_note"]
        with self.assertRaisesRegex(IntakeError, "absent"):
            validate_questions(q, notes)

    def test_no_source_family_double_counting_in_structured_mode(self):
        doc = payload()
        sources, notes = validate_collection(doc)
        q = questions()["questions"][0]
        results = search(q, "B1", notes, sources, tfidf_index(notes))
        families = [x["source_family"] for x in results["results"]]
        self.assertEqual(len(families), len(set(families)))

    def test_out_of_domain_unknowns_do_not_generate_citations(self):
        doc = payload()
        sources, notes = validate_collection(doc)
        index = tfidf_index(notes)
        for q in questions()["questions"][-2:]:
            self.assertFalse(q["gold_note_ids"])
            for mode in ("B0", "B1"):
                result = search(q, mode, notes, sources, index)
                self.assertEqual(result["results"], [])
                self.assertEqual(result["status"], "abstain_no_shared_tokens")

    def test_metadata_priority_never_creates_relevance_without_text_overlap(self):
        doc = payload()
        sources, notes = validate_collection(doc)
        q = {
            "id": "adversarial",
            "domain": "biology",
            "required_roles": ["measurement", "feedback"],
            "text": "zither quasar bicarbonate gobbledygook"
        }
        found = search(q, "B1", notes, sources, tfidf_index(notes))
        self.assertEqual(found["results"], [])

    def test_metric_ranges_and_citation_receipts(self):
        result = evaluate(payload(), questions())
        for name in ("B0", "B1"):
            m = result["methods"][name]
            self.assertTrue(0 <= m["recall_at_3"] <= 1)
            self.assertTrue(0 <= m["mrr_at_3"] <= 1)
            for trace in m["traces"]:
                for cite in trace["result"]["results"]:
                    self.assertTrue(cite["citation_url"].startswith("https://"))
                    self.assertTrue(cite["citation_anchor"])
                    self.assertEqual(cite["epistemic_status"],
                                     "paraphrase_needs_human_review")


if __name__ == "__main__":
    unittest.main(verbosity=2)
