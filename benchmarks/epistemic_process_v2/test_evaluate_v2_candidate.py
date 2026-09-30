#!/usr/bin/env python3
"""Compatibility smoke test retained for the Cycle-5 workflow.

The full Cycle-6 adversarial suite lives in test_evaluator_adversarial_v2.py.
"""
from __future__ import annotations

from test_evaluator_adversarial_v2 import fixture
from evaluate_v2_candidate import evaluate


def main() -> None:
    tasks, raw = fixture()
    result = evaluate(tasks, raw)
    assert result["aggregate"]["C1"]["evidence_retention_exactness"] == 1.0
    assert result["aggregate"]["G1"]["committed_accuracy"] == 1.0
    assert result["aggregate"]["G2"]["earned_resolution_rate"] == 1.0
    assert "C0->C1" in result["comparisons"]
    assert "G1->G2" in result["comparisons"]
    print("cycle5_evaluator_contracts=passed")


if __name__ == "__main__":
    main()
