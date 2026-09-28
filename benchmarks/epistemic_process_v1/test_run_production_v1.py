#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODULE_PATH = HERE / "run_production_v1.py"
spec = importlib.util.spec_from_file_location("run_production_v1", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

event = {
    "step": 1,
    "id": "e1",
    "family": "F_A",
    "kind": "support",
    "bears_on": "H1",
    "depends_on": [],
    "independent": True,
    "decisive": True,
}
row = module._sanitized_row(event)
assert row == "1\te1\tF_A\tsupport\tH1\t"
assert "True" not in row

task = {
    "id": "EP_TEST",
    "terminal_supported": "H2",
    "minimum_independent_families_for_resolution": 99,
    "events": [
        event,
        {
            "step": 2,
            "id": "e2",
            "family": "F_B",
            "kind": "refute",
            "bears_on": "H1",
            "depends_on": ["e1"],
            "independent": True,
            "decisive": True,
        },
    ],
}
b0 = module._b0_episode(task)
assert b0["configuration"] == "B0"
assert b0["steps"][0]["decision"]["hypothesis"] == "H1"
assert b0["steps"][1]["decision"]["hypothesis"] is None
assert b0["steps"][1]["decision"]["status"] == "abstain"

cpp = (HERE.parent.parent / "tools" / "epistemic_process_runtime_runner.cpp").read_text(
    encoding="utf-8"
)
for forbidden in ("terminal_supported", "decisive", "independent"):
    assert forbidden not in cpp, f"production C++ runner leaked scoring metadata: {forbidden}"

print("production_runtime_runner_contract_passed=true")
