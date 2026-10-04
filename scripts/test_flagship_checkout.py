"""Checkout transport must not change the frozen proof or hide a changed manifest."""
import json
import unittest
from run_flagship_proof import ROOT, scenario_hashes

class CheckoutTests(unittest.TestCase):
    def setUp(self):
        self.raw = (ROOT / 'benchmarks/flagship/scenario.json').read_bytes().replace(b'\r\n', b'\n')

    def test_lf_and_crlf_share_both_identities(self):
        self.assertEqual(scenario_hashes(self.raw), scenario_hashes(self.raw.replace(b'\n', b'\r\n')))

    def test_json_formatting_keeps_semantic_identity_but_not_frozen_identity(self):
        value, frozen, semantic = scenario_hashes(self.raw)
        _, reformatted, same_semantic = scenario_hashes(json.dumps(value).encode())
        self.assertNotEqual(frozen, reformatted)
        self.assertEqual(semantic, same_semantic)

    def test_changed_content_changes_both_identities(self):
        changed = self.raw.replace(b'known-lineage de-correlation', b'fabricated independence')
        self.assertNotEqual(changed, self.raw)
        _, frozen, semantic = scenario_hashes(self.raw)
        _, other_frozen, other_semantic = scenario_hashes(changed)
        self.assertNotEqual(frozen, other_frozen)
        self.assertNotEqual(semantic, other_semantic)

    def test_invalid_json_is_rejected(self):
        with self.assertRaises(json.JSONDecodeError):
            scenario_hashes(b'{broken')

if __name__ == '__main__':
    unittest.main()
