#!/usr/bin/env python3
"""UNSCORED production-runtime runner for Epistemic Process v2.

B0 is an intentionally stateless local baseline. G0/G1/G2 are executed by the
compiled production C++ runtime runner. Only observation fields needed to
materialise evidence are passed to production; scoring-only task metadata is
never serialized into the system-under-test input.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Mapping, Sequence

CONFIGS = ("B0", "G0", "G1", "G2")
RUNTIME_FIELDS = ("step", "id", "family", "kind", "bears_on", "depends_on", "revokes")


def _evidence_metadata(task: Mapping[str, Any]) -> Dict[str, Dict[str, Any]]:
    """Return only observation metadata that is legitimate SUT input."""
    return {
        str(event["id"]): {
            "family": str(event["family"]),
            "kind": str(event["kind"]),
            "bears_on": str(event["bears_on"]),
            "depends_on": [
                str(value) for value in (event.get("depends_on") or [])
            ],
            "revokes": [
                str(value) for value in (event.get("revokes") or [])
            ],
        }
        for event in task["events"]
    }


def load(path: str | Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def dump(path: str | Path, value: Any) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _b0_episode(task: Mapping[str, Any]) -> Dict[str, Any]:
    """Bounded/stateless baseline: only the current observation is visible."""
    started = time.perf_counter()
    steps: List[Dict[str, Any]] = []
    for event in task["events"]:
        kind = str(event["kind"])
        if kind == "support":
            decision = {
                "status": "provisional",
                "hypothesis": str(event["bears_on"]),
                "evidence_refs": [str(event["id"])],
            }
        else:
            decision = {
                "status": "abstain",
                "hypothesis": None,
                "evidence_refs": [str(event["id"])],
            }
        steps.append({"step": int(event["step"]), "decision": decision})
    return {
        "episode_id": task["id"],
        "configuration": "B0",
        "hypothesis_nodes": {"H1": 101, "H2": 102},
        "edge_to_evidence_ref": {},
        "evidence_metadata": _evidence_metadata(task),
        "steps": steps,
        "failures": [],
        "runtime_receipts": [],
        "execution_seconds": time.perf_counter() - started,
    }


def _sanitized_row(event: Mapping[str, Any]) -> str:
    depends = ",".join(str(value) for value in (event.get("depends_on") or []))
    revokes = ",".join(str(value) for value in (event.get("revokes") or []))
    values = [
        str(event["step"]),
        str(event["id"]),
        str(event["family"]),
        str(event["kind"]),
        str(event["bears_on"]),
        depends,
        revokes,
    ]
    if any("\t" in value or "\n" in value for value in values):
        raise ValueError("observation field contains unsupported TSV delimiter")
    return "\t".join(values)


def _runtime_episode(
    task: Mapping[str, Any],
    configuration: str,
    runtime_runner: Path,
    seed: int,
    work_root: Path,
) -> Dict[str, Any]:
    episode_id = str(task["id"])
    safe_id = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in episode_id)
    sanitized = work_root / f"{configuration}-{safe_id}.tsv"
    sanitized.write_text(
        "\n".join(_sanitized_row(event) for event in task["events"]) + "\n",
        encoding="utf-8",
    )
    db_dir = work_root / f"db-{configuration}-{safe_id}"
    started = time.perf_counter()
    proc = subprocess.run(
        [
            str(runtime_runner),
            configuration,
            episode_id,
            str(sanitized),
            str(db_dir),
            str(seed),
        ],
        capture_output=True,
        text=True,
    )
    elapsed = time.perf_counter() - started
    if proc.returncode != 0:
        return {
            "episode_id": episode_id,
            "configuration": configuration,
            "hypothesis_nodes": {},
            "edge_to_evidence_ref": {},
            "steps": [],
            "failures": [
                f"production runtime exit={proc.returncode}",
                proc.stderr.strip(),
            ],
            "runtime_receipts": [],
            "execution_seconds": elapsed,
        }
    try:
        value = json.loads(proc.stdout)
        # Preserve the exact non-oracle observation metadata supplied to the
        # production runner. The adapter may use it only when a runtime edge
        # maps back to the corresponding evidence id.
        value["evidence_metadata"] = _evidence_metadata(task)
        value["execution_seconds"] = elapsed
    except json.JSONDecodeError as exc:
        return {
            "episode_id": episode_id,
            "configuration": configuration,
            "hypothesis_nodes": {},
            "edge_to_evidence_ref": {},
            "steps": [],
            "failures": [
                f"production runtime emitted malformed JSON: {exc}",
                proc.stdout[-2000:],
            ],
            "runtime_receipts": [],
            "execution_seconds": elapsed,
        }
    return value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--runtime-runner", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--configuration", choices=CONFIGS, action="append")
    parser.add_argument("--seed", type=int, default=20260927)
    parser.add_argument(
        "--mode",
        choices=["unscored-dry-run", "score"],
        default="unscored-dry-run",
    )
    parser.add_argument(
        "--freeze-manifest",
        help="required in score mode; verified by score_gate_v2.py",
    )
    args = parser.parse_args()

    if args.mode == "score":
        if not args.freeze_manifest:
            raise SystemExit("score-bearing execution blocked: freeze manifest is required")
        from score_gate_v2 import verify_score_gate
        gate = verify_score_gate(Path(args.freeze_manifest))
        if not gate["valid"]:
            raise SystemExit("score-bearing execution blocked: " + "; ".join(gate["errors"]))

    tasks = load(args.tasks)
    configs: Sequence[str] = tuple(args.configuration or CONFIGS)
    runtime_runner = Path(args.runtime_runner).resolve()
    if not runtime_runner.exists():
        raise SystemExit(f"runtime runner does not exist: {runtime_runner}")

    episodes: List[Dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="moolbase-epistemic-process-") as td:
        work_root = Path(td)
        for config in configs:
            for task in tasks["episodes"]:
                if config == "B0":
                    episodes.append(_b0_episode(task))
                else:
                    episodes.append(
                        _runtime_episode(
                            task, config, runtime_runner, args.seed, work_root
                        )
                    )

    result = {
        "protocol": tasks["protocol"],
        "mode": args.mode,
        "runner": "production-runtime-v2-instrumented",
        "score_bearing": args.mode == "score",
        "seed": args.seed,
        "runtime_input_fields": list(RUNTIME_FIELDS),
        "episodes": episodes,
    }
    dump(args.out, result)
    print(
        json.dumps(
            {
                "episodes": len(episodes),
                "configurations": list(configs),
                "runtime_failures": sum(bool(ep["failures"]) for ep in episodes),
                "score_bearing": args.mode == "score",
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
