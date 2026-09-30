#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

import score_gate_v2


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ScoreGateTests(unittest.TestCase):
    def _fixture(self, root: Path):
        files = {
            "tasks.json": b"tasks\n",
            "evaluator.py": b"evaluator\n",
            "adapter.py": b"adapter\n",
            "runtime.cpp": b"runtime\n",
        }
        for name, payload in files.items():
            (root / name).write_bytes(payload)
        subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)
        subprocess.run(
            ["git", "config", "user.email", "score-gate@example.invalid"],
            cwd=root, check=True,
        )
        subprocess.run(
            ["git", "config", "user.name", "Score Gate Test"],
            cwd=root, check=True,
        )
        subprocess.run(["git", "add", "."], cwd=root, check=True)
        subprocess.run(
            ["git", "commit", "-m", "freeze candidate"],
            cwd=root, check=True, capture_output=True,
        )
        candidate = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=root, check=True, capture_output=True, text=True,
        ).stdout.strip()
        candidate_tree = subprocess.run(
            ["git", "rev-parse", "HEAD^{tree}"],
            cwd=root, check=True, capture_output=True, text=True,
        ).stdout.strip()

        manifest = {
            "schema": "epistemic-process-score-freeze-v2",
            "status": "FROZEN",
            "score_bearing_authorized": True,
            "experiment_id": "TEST-SCORE-001",
            "production_architecture_commit": candidate,
            "benchmark_candidate_commit": candidate,
            "benchmark_candidate_tree": candidate_tree,
            "allowed_changes_after_candidate": [],
            "frozen_files": {
                name: sha256(root / name) for name in sorted(files)
            },
            "configuration_profiles": {
                "B0": {"graphene": False, "hypothesis": False, "dwm": False},
                "G0": {"graphene": True, "hypothesis": False, "dwm": False},
                "G1": {"graphene": True, "hypothesis": True, "dwm": False},
                "G2": {"graphene": True, "hypothesis": True, "dwm": True},
            },
            "policies": {
                "seed_policy": "fixed",
                "repetition_policy": "single",
                "timeout_policy": "preserve",
                "runtime_failure_policy": "preserve_exclude_primary",
                "adapter_failure_policy": "preserve_exclude_primary",
                "missing_receipt_policy": "preserve_exclude_primary",
                "abstention_policy": "report_with_coverage",
            },
        }
        path = root / "freeze.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        return path, manifest

    def test_valid_manifest_is_the_only_authorization(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            path, _ = self._fixture(root)
            result = score_gate_v2.verify_score_gate(path, root=root)
            self.assertTrue(result["valid"])
            self.assertEqual(result["errors"], [])

    def test_modified_frozen_artifact_is_rejected(self):
        for target in ("tasks.json", "evaluator.py", "adapter.py", "runtime.cpp"):
            with self.subTest(target=target):
                with tempfile.TemporaryDirectory() as td:
                    root = Path(td)
                    path, _ = self._fixture(root)
                    (root / target).write_text("tampered\n", encoding="utf-8")
                    result = score_gate_v2.verify_score_gate(path, root=root)
                    self.assertFalse(result["valid"])
                    self.assertTrue(
                        any(target in error and "hash mismatch" in error
                            for error in result["errors"])
                    )

    def test_unfrozen_or_unauthorized_manifest_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            path, manifest = self._fixture(root)
            manifest["status"] = "PREFREEZE"
            manifest["score_bearing_authorized"] = False
            path.write_text(json.dumps(manifest), encoding="utf-8")
            result = score_gate_v2.verify_score_gate(path, root=root)
            self.assertFalse(result["valid"])
            self.assertIn(
                "freeze manifest status is not FROZEN", result["errors"]
            )
            self.assertIn(
                "freeze manifest does not authorize score-bearing execution",
                result["errors"],
            )

    def test_missing_profile_or_policy_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            path, manifest = self._fixture(root)
            del manifest["configuration_profiles"]["G2"]
            del manifest["policies"]["timeout_policy"]
            path.write_text(json.dumps(manifest), encoding="utf-8")
            result = score_gate_v2.verify_score_gate(path, root=root)
            self.assertFalse(result["valid"])
            self.assertTrue(
                any("B0/G0/G1/G2" in error for error in result["errors"])
            )
            self.assertTrue(
                any("timeout_policy" in error for error in result["errors"])
            )


if __name__ == "__main__":
    unittest.main()
