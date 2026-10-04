"""Fail-closed checks for the preregistered distinction between search and challenge."""
import unittest
from run_flagship_perturbations_v2 import evaluate_contract

class ContractTests(unittest.TestCase):
    def test_copied_family_cannot_earn_corroboration(self):
        observation = dict(raw_paths=3, independent_families=1,
            sufficient_independent_support=False, requires_external_verification=True,
            corroboration_search_required=True, dialectical_challenge=False,
            opposition_requests_reexpansion=False)
        self.assertEqual(evaluate_contract('P1', observation), [])
        for key, value in [('independent_families', 2), ('sufficient_independent_support', True),
                           ('corroboration_search_required', False), ('dialectical_challenge', True),
                           ('opposition_requests_reexpansion', True)]:
            with self.subTest(key=key):
                self.assertTrue(evaluate_contract('P1', dict(observation, **{key: value})))

    def test_removal_requires_search_without_inventing_opposition(self):
        observation = dict(baseline_independent_families=2, perturbed_independent_families=1,
            baseline_sufficient_independent_support=True, perturbed_sufficient_independent_support=False,
            baseline_requires_external_verification=False, perturbed_requires_external_verification=True,
            perturbed_corroboration_search_required=True, perturbed_dialectical_challenge=False,
            perturbed_opposition_requests_reexpansion=False)
        self.assertEqual(evaluate_contract('P2', observation), [])
        for key in ['perturbed_corroboration_search_required', 'perturbed_dialectical_challenge',
                    'perturbed_opposition_requests_reexpansion']:
            with self.subTest(key=key):
                self.assertTrue(evaluate_contract('P2', dict(observation, **{key: not observation[key]})))

    def test_material_opposition_requires_actual_challenge(self):
        observation = dict(contradiction_blocks_resolution=True, admissible=False,
                           opposition_requests_reexpansion=True, dialectical_challenge=True)
        self.assertEqual(evaluate_contract('P3', observation), [])
        self.assertTrue(evaluate_contract('P3', dict(observation, dialectical_challenge=False)))

    def test_missing_fields_and_unknown_case_fail_closed(self):
        for pid in ['P1', 'P2', 'P3', 'P4', 'P5', 'unknown']:
            with self.subTest(pid=pid):
                self.assertTrue(evaluate_contract(pid, {}))

    def test_budget_widening_is_rejected(self):
        observation = dict(budget_max_recursive_cycles=1, expansion_rounds=1,
            trace_count=1, semantic_widening=False, hop_step_valid=True)
        self.assertEqual(evaluate_contract('P5', observation), [])
        self.assertTrue(evaluate_contract('P5', dict(observation, semantic_widening=True)))
        self.assertTrue(evaluate_contract('P5', dict(observation, expansion_rounds=2)))

if __name__ == '__main__':
    unittest.main()
