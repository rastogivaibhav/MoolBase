#!/usr/bin/env python3
"""Run the deterministic GrapheneDB epistemic flagship proof.

Builds the canonical offline demo, executes it, validates the negative-control
contract, and writes machine-readable + human-readable reproduction artifacts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]


def checked(cmd: list[str], *, cwd: pathlib.Path = ROOT) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout or "")
        sys.stderr.write(completed.stderr or "")
        raise SystemExit(completed.returncode)
    return completed


def convert(value: str) -> Any:
    if value == "true":
        return True
    if value == "false":
        return False
    if value in {"none", "continue"}:
        return value
    try:
        if "." in value:
            return float(value)
        return int(value)
    except ValueError:
        return value


def parse_line(line: str) -> tuple[str, dict[str, Any]]:
    parts = line.strip().split("|")
    kind = parts[0]
    fields: dict[str, Any] = {}
    if kind == "PHASE":
        fields["phase"] = parts[1]
        rest = parts[2:]
    else:
        rest = parts[1:]
    for item in rest:
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        fields[key] = convert(value)
    return kind, fields


def git_head() -> str:
    return checked(["git", "rev-parse", "HEAD"]).stdout.strip()


def canonical_hash(value: Any) -> str:
    encoded = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FLAGSHIP_CONTRACT_FAILURE: {message}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-flagship-proof")
    parser.add_argument("--output-dir", default="reports/flagship-proof")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve()
    output_dir = (ROOT / args.output_dir).resolve()
    manifest_path = ROOT / "benchmarks/flagship/scenario.json"

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
            "--target", "graphenedb_epistemic_flagship_demo",
        ])

    binary_name = (
        "graphenedb_epistemic_flagship_demo.exe"
        if os.name == "nt"
        else "graphenedb_epistemic_flagship_demo"
    )
    binary = build_dir / binary_name
    require(binary.exists(), f"demo binary not found: {binary}")

    run = checked([str(binary)])
    raw_output = run.stdout
    records = [parse_line(line) for line in raw_output.splitlines() if line.strip()]

    phases: dict[str, dict[str, Any]] = {}
    dwm: dict[str, Any] | None = None
    recovery: dict[str, Any] | None = None
    traces: list[dict[str, Any]] = []
    history: dict[str, Any] | None = None
    contract: dict[str, Any] | None = None

    for kind, fields in records:
        if kind == "PHASE":
            phases[str(fields.pop("phase"))] = fields
        elif kind == "DWM":
            dwm = fields
        elif kind == "RECOVERY":
            recovery = fields
        elif kind == "TRACE":
            traces.append(fields)
        elif kind == "HISTORY":
            history = fields
        elif kind == "FLAGSHIP":
            contract = fields

    require(set(phases) == {"A", "B", "C", "E"}, "missing canonical phase")
    require(dwm is not None, "missing DWM record")
    require(recovery is not None, "missing recovery record")
    require(history is not None, "missing history record")
    require(contract is not None and contract.get("contract_passed") is True,
            "C++ flagship contract did not pass")

    # A: graph multiplicity cannot become fabricated independence.
    require(phases["A"]["raw_paths"] == 2, "phase A raw path count changed")
    require(phases["A"]["independent_families"] == 1,
            "phase A correlated evidence inflated independence")
    require(phases["A"]["sufficient_independent_support"] is False,
            "phase A incorrectly earned corroboration")

    # B: the alternative remains visible and operationally challengeable.
    require(phases["B"]["primary"] == 101, "phase B primary changed")
    require(phases["B"]["discarded_paths"] >= 1,
            "phase B competing path disappeared")
    require(phases["B"]["opposition_requests_reexpansion"] is True,
            "phase B alternative did not drive opposition")
    require(phases["B"]["reopen_count"] >= 2,
            "phase B did not retain both reopen targets")

    # C: negative control. More evidence includes decisive opposition, so the
    # correct result is refusal of final resolution.
    require(phases["C"]["contradiction_blocks_resolution"] is True,
            "phase C contradiction did not block resolution")
    require(phases["C"]["admissible"] is False,
            "phase C contradiction remained admissible")
    require(phases["C"]["opposition_requests_reexpansion"] is True,
            "phase C did not request challenge/reopen")

    # D: operational reopen + defect-specific depth recovery.
    require(dwm["has_reopened_bundle"] is True, "DWM did not reopen")
    require(dwm["rounds"] == 1, "DWM bounded reopen count changed")
    require(dwm["durable_writes"] is False,
            "DWM unexpectedly performed durable writes")
    require(recovery["expansion_rounds"] == 3,
            "frontier recovery no longer uses three bounded expansions")
    require(len(traces) == 3, "recovery trace round count changed")
    for index, trace in enumerate(traces):
        require(trace["semantic_candidates_before"] == 1,
                f"round {index}: semantic candidate baseline widened")
        require(trace["semantic_candidates_after"] == 1,
                f"round {index}: semantic candidates widened during depth repair")
        require(trace["max_hops_after"] == trace["max_hops_before"] + 1,
                f"round {index}: depth frontier did not advance exactly one hop")

    # E: new discriminating evidence changes the governed evidence state but
    # does not manufacture a durable cross-run belief-revision claim.
    require(phases["E"]["primary"] == 101, "phase E primary changed")
    require(phases["E"]["sufficient_independent_support"] is True,
            "phase E did not earn independent corroboration")
    require(phases["E"]["contradiction_blocks_resolution"] is False,
            "phase E contradiction blocker unexpectedly remains")
    require(history["prior_receipt_preserved"] is True,
            "phase C receipt identity was not preserved")
    require(history["durable_cross_run_belief_revision_claim"] is False,
            "demo overclaimed durable DWM revision")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest_hash = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    commit = git_head()

    canonical = {
        "schema_version": 1,
        "scenario_id": manifest["scenario_id"],
        "commit": commit,
        "scenario_manifest_sha256": manifest_hash,
        "phases": phases,
        "dwm": dwm,
        "recovery": recovery,
        "recovery_trace": traces,
        "history": history,
        "contract": contract,
        "claim_boundary": manifest["claim_boundary"],
    }
    receipt_hash = canonical_hash(canonical)

    receipt = {
        **canonical,
        "receipt_hash_sha256": receipt_hash,
        "environment": {
            "platform": platform.system(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "environment_excluded_from_receipt_hash": True,
        },
        "raw_demo_output_sha256": hashlib.sha256(
            raw_output.encode("utf-8")
        ).hexdigest(),
    }

    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "receipt.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output_dir / "scenario_manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output_dir / "raw_output.txt").write_text(raw_output, encoding="utf-8")

    summary = f"""# GrapheneDB flagship epistemic proof

Commit: `{commit}`

Receipt SHA-256: `{receipt_hash}`

Scenario: **{manifest['title']}**

## What the run demonstrated

- **Phase A — correlated evidence:** {phases['A']['raw_paths']} raw paths remained only {phases['A']['independent_families']} independent evidence family; independent corroboration was not fabricated.
- **Phase B — competing hypothesis:** H2 remained visible after H1 was selected; opposition requested bounded re-expansion with {phases['B']['reopen_count']} reopen targets.
- **Phase C — negative control:** material contradiction blocked resolution and made the evidence inadmissible for final resolution. Success here means GrapheneDB **refused to converge**.
- **Phase D — DWM/recovery:** bounded dialectic produced a reopened bundle with no durable writes. Missing-hop recovery executed {recovery['expansion_rounds']} depth expansions without widening semantic candidates.
- **Phase E — discriminating evidence:** independent support increased and the contradiction blocker cleared while the earlier Phase C receipt hash remained recorded.

## Product mapping

- **GrapheneDB:** evidence/lineage state, FiberBundles, admissibility and durable substrate.
- **HypoKosh:** competing-hypothesis/recovery runtime.
- **DWM:** bounded opposition, reopen and synthesis.

## How to reproduce

From a clean checkout:

```bash
python3 scripts/run_flagship_proof.py
```

Artifacts are written to `reports/flagship-proof/`.

## How to falsify this demo

Try these perturbations and report any invariant violation:

1. duplicate a support path but keep the same source/evidence family — independent support must not increase;
2. remove one genuinely independent support family — corroboration must fall rather than remain inflated;
3. inject material contradiction — final resolution must remain blocked;
4. reorder the deterministic evidence insertion/path order — canonical FiberBundle/receipt behaviour should remain stable;
5. reduce or alter the permitted depth/search budget — the recovery receipt must show the changed frontier/stop decision rather than silently broadening retrieval.

A bug is especially valuable if one of these changes causes silent promotion, contradiction loss, fabricated independence, or an unexplained receipt change.

## Claim boundary

This controlled demo supports only the mechanisms listed in `benchmarks/flagship/scenario.json`.

It does **not** establish semantic truth, hidden-dependence discovery, autonomous scientific discovery, durable cross-run DWM belief promotion/revision, general superiority over external systems, or enterprise readiness.
"""
    (output_dir / "summary.md").write_text(summary, encoding="utf-8")

    print(json.dumps({
        "flagship_proof": "PASS",
        "commit": commit,
        "receipt_hash_sha256": receipt_hash,
        "receipt": str((output_dir / "receipt.json").relative_to(ROOT)),
        "summary": str((output_dir / "summary.md").relative_to(ROOT)),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
