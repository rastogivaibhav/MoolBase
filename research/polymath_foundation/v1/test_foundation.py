#!/usr/bin/env python3
"""Adversarial Polymath Foundation v1 metadata / triage regression tests."""
import copy
import json
import tempfile
import unittest
from pathlib import Path

from foundation import ContractError, main, priority, triage, validate_package

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "three_domain_fixture.json"


def source_package():
    return json.loads(FIXTURE.read_text(encoding="utf-8"))


class FoundationTests(unittest.TestCase):
    def test_baseline_domains_and_deterministic_rank(self):
        p = source_package()
        a = triage(p)
        b = triage(p)
        self.assertEqual(a, b)
        self.assertEqual(a["counts"]["domains"], 3)
        self.assertEqual(a["counts"]["sources"], 3)
        self.assertEqual(a["counts"]["candidate_questions"], 3)
        self.assertEqual(a["counts"]["blocked_mappings"], 1)
        self.assertEqual(a["epistemic_status"], "no_new_verified_facts")
        self.assertEqual(set(a["domain_competence"].values()), {"not_assessed"})
        self.assertEqual(a["agenda"][0]["id"], "map-network-supply")
        self.assertTrue(all(not r["external_experiment_permission"]
                            for r in a["agenda"]))
        self.assertTrue(all(r["status"] in ("candidate_unverified", "needs_research")
                            for r in a["agenda"]))
        self.assertEqual(a["blocked"],
                         [{"id": "map-flow-word-only",
                           "reason": "LEXICAL_MATCH_NOT_STRUCTURAL",
                           "status": "rejected"}])

    def test_false_source_authentication_rejected(self):
        p = source_package()
        p["sources"][0]["integrity"] = "externally_verified"
        with self.assertRaisesRegex(ContractError, "cannot claim independently"):
            validate_package(p)

    def test_missing_source_rejected(self):
        p = source_package()
        p["concepts"][0]["source_ids"] = ["missing"]
        with self.assertRaisesRegex(ContractError, "missing source"):
            validate_package(p)

    def test_ontology_scope_rejected(self):
        p = source_package()
        p["concepts"][1]["domain"] = "biology"
        with self.assertRaisesRegex(ContractError, "domain-qualified"):
            validate_package(p)

    def test_unjustified_shared_mechanism_rejected(self):
        p = source_package()
        p["mappings"][0]["shared_roles"] = ["adaptive_response"]
        with self.assertRaisesRegex(ContractError, "not shared"):
            validate_package(p)

    def test_missing_analogy_boundary_rejected(self):
        p = source_package()
        p["mappings"][0]["boundary_conditions"] = []
        with self.assertRaisesRegex(ContractError, "boundary_conditions"):
            validate_package(p)

    def test_missing_discriminating_falsifier_rejected(self):
        p = source_package()
        p["mappings"][0]["falsifier"] = ""
        with self.assertRaisesRegex(ContractError, "falsifier"):
            validate_package(p)

    def test_lexical_only_not_promoted_to_scientific_hypothesis(self):
        p = source_package()
        p["mappings"][2]["prediction"] = "invented prediction"
        with self.assertRaisesRegex(ContractError, "lexical-only match"):
            validate_package(p)

    def test_duplicate_family_defers_cross_domain_hypothesis(self):
        p = source_package()
        p["sources"][2]["family_id"] = "family-network"
        a = triage(p)
        self.assertIn({"id": "map-network-supply",
                       "reason": "SOURCE_FAMILIES_NOT_INDEPENDENT",
                       "status": "deferred"}, a["blocked"])
        self.assertNotIn("map-network-supply", [v["id"] for v in a["agenda"]])

    def test_prerequisite_cycle_rejected(self):
        p = source_package()
        p["concepts"][0]["prerequisites"] = ["networks:backpressure"]
        with self.assertRaisesRegex(ContractError, "cycle"):
            validate_package(p)

    def test_leaked_competence_answer_rejected(self):
        p = source_package()
        p["competence_tests"][0]["answer_access"] = "revealed"
        with self.assertRaisesRegex(ContractError, "held-out answer"):
            validate_package(p)

    def test_ranking_rejects_nan_and_out_of_range(self):
        p = source_package()
        p["mappings"][0]["information_gain"] = float("nan")
        with self.assertRaisesRegex(ContractError, "finite number"):
            validate_package(p)
        p = source_package()
        p["mappings"][0]["risk"] = 3.0
        with self.assertRaisesRegex(ContractError, "finite number"):
            validate_package(p)

    def test_cli_writes_immutable_receipt_and_does_not_run_tools(self):
        p = FIXTURE
        with tempfile.TemporaryDirectory() as d:
            output = Path(d) / "triage.json"
            import sys
            original = sys.argv
            try:
                sys.argv = ["foundation", "--input", str(p), "--output", str(output)]
                main()
            finally:
                sys.argv = original
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["source_sha256"]), 64)
            self.assertTrue(result["warnings"])
            self.assertEqual(result["epistemic_status"], "no_new_verified_facts")


if __name__ == "__main__":
    unittest.main(verbosity=2)
