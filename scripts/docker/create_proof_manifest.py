#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import xml.etree.ElementTree as ET


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: pathlib.Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def ctest_summary(path: pathlib.Path) -> dict | None:
    if not path.exists():
        return None
    root = ET.parse(path).getroot()
    tests = int(root.attrib.get("tests", 0))
    failures = int(root.attrib.get("failures", 0))
    errors = int(root.attrib.get("errors", 0))
    skipped = int(root.attrib.get("skipped", 0))
    return {
        "tests": tests,
        "failures": failures,
        "errors": errors,
        "skipped": skipped,
        "passed": tests - failures - errors - skipped,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--status", choices=["PASS", "FAIL"], required=True)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--started-at", required=True)
    parser.add_argument("--finished-at", required=True)
    args = parser.parse_args()

    root = pathlib.Path(args.output_dir).resolve()
    files = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.name in {
            "checksums.sha256",
            "proof-manifest.json",
            "proof-summary.md",
        }:
            continue
        files.append(
            {
                "path": path.relative_to(root).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
        )

    intervention = None
    for candidate in root.rglob("summary.json"):
        data = load_json(candidate)
        if isinstance(data, dict) and "policy_summary" in data and "gates" in data:
            intervention = {
                "path": candidate.relative_to(root).as_posix(),
                "total_runs": data.get("total_runs"),
                "all_gates_pass": data.get("all_gates_pass"),
                "policy_summary": data.get("policy_summary"),
                "gates": data.get("gates"),
            }
            break

    cross_dataset = []
    for candidate in root.rglob("*_summary.json"):
        data = load_json(candidate)
        if isinstance(data, dict) and (
            "all_gates_pass" in data or "gates" in data
        ):
            cross_dataset.append(
                {
                    "path": candidate.relative_to(root).as_posix(),
                    "all_gates_pass": data.get("all_gates_pass"),
                    "gates": data.get("gates"),
                    "examples": data.get("examples") or data.get("total_examples"),
                }
            )

    critical_ctest = ctest_summary(root / "critical-ctest.xml")
    manifest = {
        "schema_version": 1,
        "status": args.status,
        "mode": args.mode,
        "source_commit": args.source_commit,
        "source_ref": args.source_ref,
        "started_at": args.started_at,
        "finished_at": args.finished_at,
        "critical_ctest": critical_ctest,
        "intervention": intervention,
        "cross_dataset": cross_dataset,
        "files": files,
        "claim_boundary": {
            "proves": [
                "the exact packaged source can configure and compile in the declared container",
                "the selected deterministic tests and benchmark gates completed without process failure",
                "the evidence bundle is content-addressed and tied to the supplied source commit",
            ],
            "does_not_prove": [
                "semantic truth or universal reasoning accuracy",
                "global Lyapunov convergence",
                "production security, availability, or enterprise GA readiness",
                "independence unless a separate person or environment runs the bundle",
            ],
        },
    }
    (root / "proof-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    lines = [
        "# GrapheneDB Docker proof summary",
        "",
        f"- Status: **{args.status}**",
        f"- Mode: `{args.mode}`",
        f"- Source commit: `{args.source_commit}`",
        f"- Source ref: `{args.source_ref}`",
        f"- Started: `{args.started_at}`",
        f"- Finished: `{args.finished_at}`",
        "",
    ]
    if critical_ctest:
        lines.extend(
            [
                "## Critical CTest",
                "",
                f"- Tests: {critical_ctest['tests']}",
                f"- Passed: {critical_ctest['passed']}",
                f"- Failures: {critical_ctest['failures']}",
                f"- Errors: {critical_ctest['errors']}",
                "",
            ]
        )
    if intervention:
        targeted = intervention["policy_summary"].get("targeted_frontier_aware", {})
        broad = intervention["policy_summary"].get("broad_forced_3", {})
        lines.extend(
            [
                "## Controlled intervention benchmark",
                "",
                f"- Total executions: {intervention.get('total_runs')}",
                f"- All frozen gates passed: {intervention.get('all_gates_pass')}",
                f"- Frontier-aware accuracy: {targeted.get('final_accuracy')}",
                f"- Frontier-aware mean visited states: {targeted.get('mean_visited')}",
                f"- Broad accuracy: {broad.get('final_accuracy')}",
                f"- Broad mean visited states: {broad.get('mean_visited')}",
                "",
            ]
        )
    lines.extend(
        [
            "## Claim boundary",
            "",
            "This bundle demonstrates reproducible build, test and controlled mechanism behaviour for the supplied source commit. It does not establish semantic truth, universal reasoning superiority, formal global convergence, or production readiness.",
            "",
        ]
    )
    (root / "proof-summary.md").write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
