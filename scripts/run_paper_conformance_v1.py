#!/usr/bin/env python3
"""Executable paper->system conformance runner for GrapheneDB v1.

This runner intentionally distinguishes executable proof from architectural gaps.
A green process exit means the harness executed without a failing mapped test;
it does NOT mean the paper/system conformance gate is complete.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[1]

TRAJECTORIES = {
    "T1": {
        "title": "Duplicate-family false convergence",
        "tests": ["graphenedb_fiber_bundle_v2_tests", "graphenedb_epistemic_control_tests"],
        "claims": ["C01", "C02"],
        "success_status": "PROVEN",
        "boundary": "Known duplicate/shared-source evidence is de-correlated; hidden dependence remains outside the current claim.",
    },
    "T2": {
        "title": "Contradiction and safe non-convergence",
        "tests": ["graphenedb_epistemic_control_tests", "graphenedb_dialectic_tests"],
        "claims": ["C03", "C04"],
        "success_status": "PROVEN",
        "boundary": "This proves contradiction visibility/blocking and preservation of alternatives in the bounded reasoning path, not semantic truth.",
    },
    "T3": {
        "title": "Operational opposition -> bounded targeted reopen",
        "tests": ["graphenedb_dialectic_tests", "graphenedb_hypokosh_runtime_tests"],
        "claims": ["C05", "C14"],
        "success_status": "PROVEN",
        "boundary": (
            "This is the current-paper claim: opposition/reopen targets influence bounded subsequent retrieval and synthesis remains read-only. "
            "Durable accepted-belief revision across later world updates is a broader future architecture claim, not a current-paper requirement."
        ),
    },
    "T4": {
        "title": "Hypothesis proposal cannot become fact by generation",
        "tests": ["graphenedb_governed_learning_tests", "graphenedb_hypokosh_runtime_tests"],
        "claims": ["C06", "C07", "C08", "C14"],
        "success_status": "PROVEN",
        "boundary": "Generated hypotheses remain hypothetical/read-only; this does not establish autonomous truth promotion.",
    },
    "T5": {
        "title": "Governed retrieval-policy promotion and rollback",
        "tests": ["graphenedb_governed_learning_tests"],
        "claims": ["C09", "C10"],
        "success_status": "PROVEN",
        "boundary": (
            "This proves only the governed-learning specification's retrieval-policy promotion/rollback and append-only history. "
            "Durable DWM model-world belief promotion remains future work."
        ),
    },
    "T6": {
        "title": "Deterministic receipt/replay",
        "tests": ["graphenedb_epistemic_receipt_tests", "graphenedb_hypokosh_runtime_tests"],
        "claims": ["C12"],
        "success_status": "PROVEN",
        "boundary": "Determinism is established for the mapped receipt/runtime fixtures, not for every future external model/tool integration.",
    },
    "T7": {
        "title": "Frontier-aware defect-specific recovery",
        "tests": ["graphenedb_hypokosh_runtime_tests"],
        "claims": ["C13"],
        "success_status": "PROVEN",
        "boundary": (
            "The current paper claims controlled mechanism behaviour only. T7 additionally requires the frozen intervention benchmark gates; "
            "it does not establish semantic truth or real-world generalisation."
        ),
    },
}


def sha256_file(path: pathlib.Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def git_head() -> str | None:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return None


def run_test(build_dir: pathlib.Path, name: str) -> dict:
    cmd = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "-R",
        f"^{name}$",
        "--output-on-failure",
    ]
    completed = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True)
    output = (completed.stdout or "") + (completed.stderr or "")
    return {
        "name": name,
        "command": " ".join(cmd),
        "returncode": completed.returncode,
        "passed": completed.returncode == 0,
        "output": output[-12000:],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-paper-conformance")
    parser.add_argument(
        "--output",
        default="reports/paper-conformance-v1/report.json",
    )
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve()
    output_path = (ROOT / args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    unique_tests = sorted(
        {test for trajectory in TRAJECTORIES.values() for test in trajectory["tests"]}
    )
    test_results = {name: run_test(build_dir, name) for name in unique_tests}

    intervention_path = ROOT / "reports/dialectic_intervention/summary.json"
    intervention = None
    if intervention_path.exists():
        try:
            intervention = json.loads(intervention_path.read_text(encoding="utf-8"))
        except Exception:
            intervention = None

    trajectories = {}
    any_failure = False
    for tid, spec in TRAJECTORIES.items():
        mapped = [test_results[name] for name in spec["tests"]]
        if any(not result["passed"] for result in mapped):
            status = "CONTRADICTED_OR_BROKEN"
            any_failure = True
        else:
            status = spec["success_status"]
        if tid == "T7":
            if intervention is None or not intervention.get("all_gates_pass", False):
                status = "EVIDENCE_MISSING_OR_FAILED"
                any_failure = True
        trajectories[tid] = {
            "title": spec["title"],
            "claims": spec["claims"],
            "status": status,
            "boundary": spec["boundary"],
            "tests": spec["tests"],
        }

    current_paper_ids = {"T1", "T2", "T3", "T6", "T7"}
    failing_statuses = {"PARTIAL", "CONTRADICTED_OR_BROKEN", "EVIDENCE_MISSING_OR_FAILED"}
    current_paper_incomplete = any(
        trajectories[tid]["status"] in failing_statuses for tid in current_paper_ids
    )
    extended_incomplete = any(
        item["status"] in failing_statuses for item in trajectories.values()
    )

    report = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "repository": "rastogivaibhav/graphenedb_v1",
        "commit": git_head(),
        "gate": "paper-system-conformance-v1",
        "gate_status": "INCOMPLETE" if extended_incomplete else "COMPLETE",
        "current_paper_gate_status": "INCOMPLETE" if current_paper_incomplete else "COMPLETE",
        "extended_architecture_gate_status": "INCOMPLETE" if extended_incomplete else "COMPLETE",
        "interpretation": (
            "COMPLETE means all mapped v1 trajectories are executable at their stated claim boundaries. "
            "It does not establish semantic truth, external superiority, independent usability, external validation, or adoption."
        ),
        "intervention_benchmark": intervention,
        "source_hashes": {
            "paper/main.tex": sha256_file(ROOT / "paper/main.tex"),
            "paper/CLAIM_EVIDENCE_MATRIX.md": sha256_file(ROOT / "paper/CLAIM_EVIDENCE_MATRIX.md"),
            "docs/DIALECTIC_REASONING_V0.md": sha256_file(ROOT / "docs/DIALECTIC_REASONING_V0.md"),
            "docs/GOVERNED_LEARNING_V0_SPEC.md": sha256_file(ROOT / "docs/GOVERNED_LEARNING_V0_SPEC.md"),
            "docs/lab/PAPER_SYSTEM_CONFORMANCE_V1.md": sha256_file(ROOT / "docs/lab/PAPER_SYSTEM_CONFORMANCE_V1.md"),
        },
        "trajectories": trajectories,
        "test_results": test_results,
        "known_gaps": [
            {
                "id": "GAP-DWM-DURABLE-REVISION",
                "severity": "FUTURE_WORK",
                "description": (
                    "Current DWM/dialectic is read-only and does not durably commit an accepted model-world belief. "
                    "Therefore durable late-counterevidence reopening/revision cannot yet be claimed as proven."
                ),
                "blocks_current_paper": [],
                "extended_architecture_gap": ["durable cross-run model-world belief revision"],
            },
            {
                "id": "GAP-POLICY-VS-BELIEF-PROMOTION",
                "severity": "FUTURE_WORK",
                "description": (
                    "Governed-learning tests prove retrieval-policy promotion/rollback, not durable model-world belief promotion."
                ),
                "blocks_current_paper": [],
                "extended_architecture_gap": ["durable DWM model-world belief promotion"],
            },
        ],
    }

    output_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "gate_status": report["gate_status"],
        "current_paper_gate_status": report["current_paper_gate_status"],
        "extended_architecture_gate_status": report["extended_architecture_gate_status"],
        "report": str(output_path.relative_to(ROOT)),
        "trajectory_statuses": {k: v["status"] for k, v in trajectories.items()},
    }, sort_keys=True))

    return 1 if any_failure else 0


if __name__ == "__main__":
    sys.exit(main())
