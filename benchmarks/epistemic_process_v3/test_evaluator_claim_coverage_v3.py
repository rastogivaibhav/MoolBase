#!/usr/bin/env python3
from __future__ import annotations
import json
from pathlib import Path

from claim_gate_v3 import evaluate_claims
from evaluate_v3_candidate import evaluate
from test_evaluator_adversarial_v3 import fixture

ROOT=Path(__file__).resolve().parent

def main():
    tasks,raw=fixture()
    result=evaluate(tasks,raw)
    gates=json.loads((ROOT/"claim_gates_v3.json").read_text())
    # Missing evaluator-derived metrics must fail here via KeyError.
    evaluated=evaluate_claims(result["gate_metrics"],gates)
    assert set(evaluated["claims"])==set(gates["claims"])
    assert all(v["status"] in {"EARNED","NOT_EARNED","INELIGIBLE_DESIGN"} for v in evaluated["claims"].values())
    print("cycle6_v3_claim_metric_coverage=passed claims="+str(len(evaluated["claims"])))

if __name__=="__main__": main()
