#!/usr/bin/env python3
"""Exercise D0 diagnostics, resume safety, and formal false-pass rejection."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def run(executable: pathlib.Path, output: pathlib.Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            str(executable),
            "--output-dir",
            str(output),
            "--seed",
            "424242",
            "--graphs",
            "25",
            "--queries-per-graph",
            "4",
            "--wall-clock-seconds",
            "90",
            "--per-query-ms",
            "1000",
            *extra,
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_deepmind_g2_harness.py HARNESS")
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="graphenedb-d0-test-") as temporary:
        root = pathlib.Path(temporary)
        diagnostic = root / "diagnostic"
        first = run(executable, diagnostic)
        require(first.returncode == 0, f"diagnostic failed:\n{first.stderr}\n{first.stdout}")

        metrics = json.loads((diagnostic / "metrics.json").read_text(encoding="utf-8"))
        require(metrics["schema"] == "deepmind-g2-metrics-v1", "wrong metrics schema")
        require(metrics["queries_completed"] == 100, "diagnostic query count is wrong")
        require(metrics["unique_query_vectors"] == 100, "query vectors are duplicated")
        require(metrics["complete"] is True, "diagnostic did not complete")
        require(metrics["formal_eligible"] is False, "small diagnostic claimed formal eligibility")
        require(metrics["g2_pass"] is False, "small diagnostic falsely claimed G2")
        require(metrics["coverage"]["node_range"] == [2, 200], "node boundary coverage missing")
        require(metrics["coverage"]["root_range"] == [1, 8], "root boundary coverage missing")
        require(metrics["metrics"]["root_set_recall"] == 1.0, "root oracle mismatch")
        require(metrics["metrics"]["admissible_path_recall"] == 1.0, "path oracle mismatch")
        require(metrics["metrics"]["inadmissible_path_count"] == 0, "forbidden path admitted")
        require(metrics["metrics"]["false_promotion_count"] == 0, "false promotion admitted")
        require(metrics["metrics"]["temporal_violations_admitted"] == 0, "temporal violation admitted")
        require(metrics["metrics"]["hyperedge_violations_admitted"] == 0, "hyperedge violation admitted")
        require(metrics["metrics"]["provenance_truth_mismatches"] == 0, "provenance oracle mismatch")

        raw_path = diagnostic / "raw_predictions.jsonl"
        state_path = diagnostic / "resume.state.tsv"
        raw_before = raw_path.read_bytes()
        state_before = state_path.read_bytes()
        resumed = run(executable, diagnostic, "--resume")
        require(resumed.returncode == 0, f"resume failed:\n{resumed.stderr}\n{resumed.stdout}")
        require(raw_path.read_bytes() == raw_before, "resume duplicated raw predictions")
        require(state_path.read_bytes() == state_before, "resume duplicated completed state")

        false_pass = root / "formal-too-small"
        rejected = run(executable, false_pass, "--formal")
        require(rejected.returncode == 2, "undersized formal run was not rejected")
        rejected_metrics = json.loads((false_pass / "metrics.json").read_text(encoding="utf-8"))
        require(rejected_metrics["formal_eligible"] is False, "undersized run became eligible")
        require(rejected_metrics["g2_pass"] is False, "undersized run claimed G2")

        prepared = root / "prepared"
        preparation = run(executable, prepared, "--prepare-only")
        require(preparation.returncode == 0, f"prepare-only failed:\n{preparation.stderr}")
        require((prepared / "dataset.jsonl").is_file(), "prepared dataset is missing")
        require((prepared / "run_config.txt").is_file(), "prepared config is missing")
        require((prepared / "preparation.json").is_file(), "preparation receipt is missing")
        require(not (prepared / "raw_predictions.jsonl").exists(), "preparation evaluated queries")
        require(not (prepared / "metrics.json").exists(), "preparation produced result metrics")

    print("deepmind_g2_harness_contract_passed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
