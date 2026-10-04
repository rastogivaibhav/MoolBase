#!/usr/bin/env python3
"""Run the preregistered MoolBase V2 flagship perturbation pack.

V1 stays immutable. V2 was preregistered in 1dbb497 for the V3-aligned
canonical receipt frozen by e7465b0; this runner implements that manifest.

The runner first replays the canonical flagship proof and requires its frozen
mechanism receipt to remain unchanged. It then executes P1-P5, writes every
receipt (including failures), and exits non-zero only after all artifacts exist.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
CANONICAL_PARENT = "e7465b09f59c50d836a462601cca998d4adac25e"
CANONICAL_FLAGSHIP_HASH = (
    "12f2c843774027b33b2e81869fc24b232f849e81f84936d7d7b8b1be189fde89"
)


def checked(cmd: list[str], *, cwd: pathlib.Path = ROOT) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout or "")
        sys.stderr.write(completed.stderr or "")
        raise SystemExit(completed.returncode)
    return completed


def git_head() -> str:
    return checked(["git", "rev-parse", "HEAD"]).stdout.strip()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def canonical_hash(value: Any) -> str:
    encoded = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")
    return sha256_bytes(encoded)


def convert(value: str) -> Any:
    if value == "true":
        return True
    if value == "false":
        return False
    try:
        return int(value)
    except ValueError:
        try:
            return float(value)
        except ValueError:
            return value


def parse_record(line: str) -> tuple[str, dict[str, Any]]:
    parts = line.strip().split("|")
    kind = parts[0]
    fields: dict[str, Any] = {}
    for item in parts[1:]:
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        fields[key] = convert(value)
    return kind, fields


def requirement(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def evaluate_contract(pid: str, observed: dict[str, Any]) -> list[str]:
    failures: list[str] = []

    if pid == "P1":
        requirement(observed.get("raw_paths", 0) >= 3, "raw_path_count_did_not_increase", failures)
        requirement(
            observed.get("independent_families") == 1,
            "duplicate_family_inflated_independence",
            failures,
        )
        requirement(
            observed.get("sufficient_independent_support") is False,
            "duplication_earned_independent_corroboration",
            failures,
        )
        requirement(
            observed.get("requires_external_verification") is True,
            "duplicate_only_state_did_not_require_external_verification",
            failures,
        )
        requirement(
            observed.get("corroboration_search_required") is True,
            "duplicate_only_state_did_not_request_corroboration",
            failures,
        )

    elif pid == "P2":
        requirement(
            observed.get("baseline_independent_families") == 2,
            "baseline_not_two_independent_families",
            failures,
        )
        requirement(
            observed.get("perturbed_independent_families") == 1,
            "removed_family_not_reflected_in_independence_count",
            failures,
        )
        requirement(
            observed.get("baseline_sufficient_independent_support") is True,
            "baseline_not_corroborated_under_current_rule",
            failures,
        )
        requirement(
            observed.get("perturbed_sufficient_independent_support") is False,
            "corroboration_survived_decisive_family_removal",
            failures,
        )
        requirement(
            observed.get("baseline_requires_external_verification") is False,
            "corroborated_baseline_still_required_external_verification",
            failures,
        )
        requirement(
            observed.get("perturbed_requires_external_verification") is True,
            "family_removal_did_not_restore_external_verification_requirement",
            failures,
        )
        requirement(
            observed.get("perturbed_corroboration_search_required") is True,
            "family_removal_did_not_request_corroboration",
            failures,
        )

    elif pid == "P3":
        requirement(
            observed.get("contradiction_blocks_resolution") is True,
            "material_contradiction_did_not_block_resolution",
            failures,
        )
        requirement(
            observed.get("admissible") is False,
            "contradictory_state_remained_admissible",
            failures,
        )
        requirement(
            observed.get("opposition_requests_reexpansion") is True,
            "contradiction_did_not_drive_opposition_or_reopen",
            failures,
        )

        requirement(observed.get("dialectical_challenge") is True,
                    "material_opposition_did_not_emit_dialectical_challenge", failures)

    elif pid == "P4":
        requirement(
            observed.get("semantic_equal") is True,
            "ingestion_order_changed_epistemic_outcome",
            failures,
        )
        requirement(
            observed.get("bundle_hash_equal") is True,
            "ingestion_order_changed_canonical_bundle_identity",
            failures,
        )

    elif pid == "P5":
        requirement(
            observed.get("budget_max_recursive_cycles") == 1,
            "declared_recovery_budget_missing_or_changed",
            failures,
        )
        requirement(
            int(observed.get("expansion_rounds", 999)) <= 1,
            "runtime_exceeded_recovery_cycle_budget",
            failures,
        )
        requirement(
            int(observed.get("trace_count", 999)) <= 1,
            "receipt_exceeded_recovery_cycle_budget",
            failures,
        )
        requirement(
            observed.get("semantic_widening") is False,
            "depth_budget_change_caused_silent_semantic_widening",
            failures,
        )
        requirement(
            observed.get("hop_step_valid") is True,
            "recovery_frontier_did_not_advance_by_declared_hop_step",
            failures,
        )
        requirement(
            observed.get("expansion_rounds") == observed.get("trace_count"),
            "recovery_round_count_and_receipt_trace_disagree",
            failures,
        )

    else:
        failures.append("unknown_perturbation_id")

    if pid in {"P1", "P2"}:
        prefix = "perturbed_" if pid == "P2" else ""
        requirement(observed.get(prefix + "dialectical_challenge") is False,
                    "support_shortfall_became_dialectical_challenge", failures)
        requirement(observed.get(prefix + "opposition_requests_reexpansion") is False,
                    "support_shortfall_caused_dwm_reexpansion", failures)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-flagship-perturbations")
    parser.add_argument("--output-dir", default="reports/flagship-perturbations-v2")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve()
    output_dir = (ROOT / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = ROOT / "benchmarks/flagship/perturbations_v2.json"
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_hash = sha256_bytes(manifest_bytes)
    if manifest_hash != "4db203469234f73fee67a332014d6edc89a482ffbe47fd1eb96a9f0b1b0bdec7":
        raise SystemExit("PERTURBATION_PROTOCOL_FAILURE: preregistered V2 manifest changed")

    if manifest.get("canonical_parent_commit") != CANONICAL_PARENT:
        raise SystemExit("PERTURBATION_PROTOCOL_FAILURE: canonical parent changed")
    if (
        manifest.get("canonical_flagship_mechanism_receipt_sha256")
        != CANONICAL_FLAGSHIP_HASH
    ):
        raise SystemExit("PERTURBATION_PROTOCOL_FAILURE: canonical flagship hash changed")

    if not args.skip_build:
        checked([
            "cmake", "-S", ".", "-B", str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DGRAPHENEDB_BUILD_TESTS=OFF",
            "-DGRAPHENEDB_BUILD_BENCH=OFF",
            "-DGRAPHENEDB_BUILD_EXAMPLES=ON",
            "-DGRAPHENEDB_BUILD_SERVER=OFF",
        ])
        checked([
            "cmake", "--build", str(build_dir), "--parallel", "2",
            "--target",
            "graphenedb_epistemic_flagship_demo",
            "graphenedb_epistemic_flagship_perturbations",
        ])

    # Guardrail: the canonical proof must still reproduce its frozen mechanism
    # hash before perturbation results are interpreted.
    baseline_output = output_dir / "canonical-baseline"
    baseline = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/run_flagship_proof.py"),
            "--skip-build",
            "--build-dir", str(build_dir),
            "--output-dir", str(baseline_output),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    (output_dir / "canonical_baseline_stdout.txt").write_text(
        baseline.stdout or "", encoding="utf-8"
    )
    (output_dir / "canonical_baseline_stderr.txt").write_text(
        baseline.stderr or "", encoding="utf-8"
    )
    if baseline.returncode != 0:
        raise SystemExit(
            "PERTURBATION_PROTOCOL_FAILURE: canonical flagship no longer reproduces"
        )

    baseline_receipt = json.loads(
        (baseline_output / "receipt.json").read_text(encoding="utf-8")
    )
    if baseline_receipt.get("receipt_hash_sha256") != CANONICAL_FLAGSHIP_HASH:
        raise SystemExit(
            "PERTURBATION_PROTOCOL_FAILURE: canonical mechanism receipt drifted"
        )

    binary = build_dir / "graphenedb_epistemic_flagship_perturbations"
    if sys.platform.startswith("win"):
        binary = build_dir / "graphenedb_epistemic_flagship_perturbations.exe"
    if not binary.exists():
        raise SystemExit(f"PERTURBATION_PROTOCOL_FAILURE: binary not found: {binary}")

    run = checked([str(binary)])
    raw_output = run.stdout
    (output_dir / "raw_output.txt").write_text(raw_output, encoding="utf-8")

    observed: dict[str, dict[str, Any]] = {}
    harness_completed = False
    for line in raw_output.splitlines():
        if not line.strip():
            continue
        kind, fields = parse_record(line)
        if kind == "PERTURB":
            pid = str(fields.get("id", ""))
            observed[pid] = fields
        elif kind == "PERTURBATION_HARNESS":
            harness_completed = fields.get("completed") is True

    expected_ids = [item["id"] for item in manifest["perturbations"]]
    if sorted(observed) != sorted(expected_ids) or not harness_completed:
        raise SystemExit(
            "PERTURBATION_PROTOCOL_FAILURE: missing perturbation records or completion marker"
        )

    spec_by_id = {item["id"]: item for item in manifest["perturbations"]}
    head = git_head()
    aggregate: list[dict[str, Any]] = []
    failures_total = 0

    receipts_dir = output_dir / "receipts"
    receipts_dir.mkdir(parents=True, exist_ok=True)

    for pid in expected_ids:
        contract_failures = evaluate_contract(pid, observed[pid])
        passed = not contract_failures
        if not passed:
            failures_total += 1

        mechanism_receipt = {
            "schema_version": 2,
            "protocol_id": manifest["protocol_id"],
            "perturbation_id": pid,
            "perturbation_name": spec_by_id[pid]["name"],
            "canonical_parent_commit": CANONICAL_PARENT,
            "canonical_flagship_mechanism_receipt_sha256": CANONICAL_FLAGSHIP_HASH,
            "perturbation_manifest_sha256": manifest_hash,
            "expected": spec_by_id[pid]["expected"],
            "observed": observed[pid],
            "passed": passed,
            "contract_failures": contract_failures,
            "failure_priority_if_violated": spec_by_id[pid]["failure_priority"],
            "result_preserved_even_when_failed": True,
        }
        mechanism_hash = canonical_hash(mechanism_receipt)
        provenance_hash = canonical_hash({
            "current_commit": head,
            "mechanism_receipt_sha256": mechanism_hash,
        })
        receipt = {
            **mechanism_receipt,
            "current_commit": head,
            "mechanism_receipt_sha256": mechanism_hash,
            "provenance_sha256": provenance_hash,
            "mechanism_hash_excludes_current_commit": True,
        }
        final_encoded = (
            json.dumps(receipt, indent=2, sort_keys=True) + "\n"
        ).encode("utf-8")
        (receipts_dir / f"{pid}.json").write_bytes(final_encoded)

        aggregate.append({
            "id": pid,
            "name": spec_by_id[pid]["name"],
            "passed": passed,
            "failures": contract_failures,
            "mechanism_receipt_sha256": mechanism_hash,
            "provenance_sha256": provenance_hash,
        })

    aggregate_receipt = {
        "schema_version": 2,
        "protocol_id": manifest["protocol_id"],
        "canonical_parent_commit": CANONICAL_PARENT,
        "current_commit": head,
        "canonical_flagship_mechanism_receipt_sha256": CANONICAL_FLAGSHIP_HASH,
        "perturbation_manifest_sha256": manifest_hash,
        "total": len(expected_ids),
        "passed": len(expected_ids) - failures_total,
        "failed": failures_total,
        "all_results_preserved": True,
        "results": aggregate,
    }
    (output_dir / "aggregate.json").write_text(
        json.dumps(aggregate_receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    lines = [
        "# Flagship Perturbation Pack V2",
        "",
        f"Commit: `{head}`",
        "",
        f"Canonical parent: `{CANONICAL_PARENT}`",
        "",
        f"Canonical flagship mechanism receipt: `{CANONICAL_FLAGSHIP_HASH}`",
        "",
        f"Perturbation manifest SHA-256: `{manifest_hash}`",
        "",
        f"Result: **{len(expected_ids) - failures_total}/{len(expected_ids)} contracts passed**",
        "",
        "| ID | Perturbation | Result | Preserved failures |",
        "|---|---|---|---|",
    ]
    for result in aggregate:
        status = "PASS" if result["passed"] else "FAIL"
        failure_text = (
            "none" if result["passed"] else ", ".join(result["failures"])
        )
        lines.append(
            f"| {result['id']} | {result['name']} | {status} | {failure_text} |"
        )
    lines.extend([
        "",
        "A FAIL is a valid lab result. It must be converted into an implementation",
        "priority without rewriting the pre-registered perturbation contract.",
        "",
        "The canonical flagship proof was replayed before these results and its",
        "frozen mechanism receipt remained unchanged.",
        "",
    ])
    (output_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")

    print(json.dumps({
        "protocol": manifest["protocol_id"],
        "commit": head,
        "passed": len(expected_ids) - failures_total,
        "failed": failures_total,
        "aggregate": str((output_dir / "aggregate.json").relative_to(ROOT)),
        "summary": str((output_dir / "summary.md").relative_to(ROOT)),
    }, sort_keys=True))

    return 1 if failures_total else 0


if __name__ == "__main__":
    raise SystemExit(main())
