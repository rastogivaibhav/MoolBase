#!/usr/bin/env python3
"""Run an external GJ-Eval adapter over WorldShift episodes.

The adapter command reads one JSON task from stdin and writes one JSON decision
to stdout. Oracle fields never leave this harness.

Execution sharding is orchestration-only: tasks retain canonical benchmark order
and a shard selects tasks by canonical ordinal modulo shard count.
"""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import time
from typing import Any


ALLOWED_OPTIONAL = {
    "choice_probabilities",
    "ranked_hypotheses",
    "epistemic_status",
    "requested_test",
    "provider_cost",
    "receipt",
}


def load_jsonl(path: str) -> list[dict[str, Any]]:
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise SystemExit(f"{path}:{line_no}: invalid JSON: {exc}") from exc
    return rows


def public_task(world: dict[str, Any], timestep: int, state_mode: str) -> dict[str, Any]:
    visible = []
    for step in world["timeline"]:
        if int(step["timestep"]) > timestep:
            break
        visible.extend(step["observations"])

    if state_mode == "raw":
        visible_state: Any = [item.get("claim", "") for item in visible]
    else:
        visible_state = visible

    task = {
        "schema_version": 1,
        "world_id": world["world_id"],
        "timestep": timestep,
        "domain": world["domain"],
        "variant": world["variant"],
        "visible_state": visible_state,
        "choices": world["choices"],
        "tests": world.get("tests", []),
    }
    assert "oracle" not in task
    return task


def failure_prediction(world_id: str, timestep: int, system: str, status: str, latency_ms: float, message: str, input_bytes: int, output_bytes: int = 0) -> dict[str, Any]:
    return {"schema_version": 1, "world_id": world_id, "timestep": timestep, "system": system, "adapter_status": status, "root_choice": None, "act": "review", "selected_confidence": 0.0, "latency_ms": latency_ms, "input_bytes": input_bytes, "output_bytes": output_bytes, "receipt": {"adapter_error": message[:4096]}}


def normalize_success(raw: dict[str, Any], world_id: str, timestep: int, system: str, latency_ms: float, input_bytes: int, output_bytes: int) -> dict[str, Any]:
    required = ("root_choice", "act", "selected_confidence")
    missing = [key for key in required if key not in raw]
    if missing:
        raise ValueError("missing adapter fields: " + ", ".join(missing))
    prediction = {"schema_version": 1, "world_id": world_id, "timestep": timestep, "system": system, "adapter_status": "ok", "root_choice": raw["root_choice"], "act": raw["act"], "selected_confidence": float(raw["selected_confidence"]), "latency_ms": latency_ms, "input_bytes": input_bytes, "output_bytes": output_bytes}
    for key in ALLOWED_OPTIONAL:
        if key in raw:
            prediction[key] = raw[key]
    return prediction


def invoke(command: list[str], task: dict[str, Any], system: str, timeout_s: float) -> dict[str, Any]:
    encoded = json.dumps(task, sort_keys=True).encode("utf-8")
    started = time.perf_counter()
    try:
        completed = subprocess.run(command, input=encoded, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout_s, check=False)
    except subprocess.TimeoutExpired:
        elapsed = (time.perf_counter() - started) * 1000.0
        return failure_prediction(task["world_id"], task["timestep"], system, "timeout", elapsed, f"adapter timed out after {timeout_s}s", len(encoded))
    elapsed = (time.perf_counter() - started) * 1000.0
    stdout = completed.stdout or b""
    if completed.returncode != 0:
        error = (completed.stderr or b"").decode("utf-8", errors="replace")
        return failure_prediction(task["world_id"], task["timestep"], system, "error", elapsed, f"adapter exited {completed.returncode}: {error}", len(encoded), len(stdout))
    try:
        raw = json.loads(stdout.decode("utf-8"))
        if not isinstance(raw, dict):
            raise ValueError("adapter output must be a JSON object")
        return normalize_success(raw, task["world_id"], task["timestep"], system, elapsed, len(encoded), len(stdout))
    except Exception as exc:
        return failure_prediction(task["world_id"], task["timestep"], system, "error", elapsed, f"malformed adapter response: {exc}", len(encoded), len(stdout))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--adapter-cmd", required=True)
    parser.add_argument("--system", required=True)
    parser.add_argument("--state-mode", choices=("raw", "structured"), default="structured")
    parser.add_argument("--timeout-s", type=float, default=30.0)
    parser.add_argument("--output", required=True)
    parser.add_argument("--shard-count", type=int, default=1)
    parser.add_argument("--shard-index", type=int, default=0)
    args = parser.parse_args()

    if args.shard_count < 1:
        raise SystemExit("--shard-count must be >= 1")
    if not 0 <= args.shard_index < args.shard_count:
        raise SystemExit("--shard-index must satisfy 0 <= index < count")

    command = shlex.split(args.adapter_cmd)
    if not command:
        raise SystemExit("--adapter-cmd cannot be empty")

    worlds = load_jsonl(args.worlds)
    ordinal = 0
    with open(args.output, "w", encoding="utf-8") as handle:
        for world in worlds:
            for step in world["timeline"]:
                timestep = int(step["timestep"])
                selected = ordinal % args.shard_count == args.shard_index
                ordinal += 1
                if not selected:
                    continue
                task = public_task(world, timestep, args.state_mode)
                prediction = invoke(command, task, args.system, args.timeout_s)
                handle.write(json.dumps(prediction, sort_keys=True))
                handle.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
