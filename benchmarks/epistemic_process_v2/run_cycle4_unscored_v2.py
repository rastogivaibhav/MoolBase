#!/usr/bin/env python3
"""Cycle-4 unscored harness for EP-PROCESS-V2.

C0 is stateless and applies the frozen common decision head to the current
observation only.

G0E is the production Graphene evidence/provenance projection with no answer
selection.

C1 executes the exact same G0E production path, then applies the exact same
common decision head used by C0 to the evidence references exposed by Graphene.

G1 and G2 call the production runtime through the V2 diagnostic runner.

This harness has no score-bearing mode.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any, Mapping, Sequence

from common_head_v2 import common_head

CONFIGS = ("C0", "G0E", "C1", "G1", "G2")
RUNTIME_FIELDS = (
    "step",
    "id",
    "family",
    "kind",
    "bears_on",
    "depends_on",
    "revokes",
)
FORBIDDEN_RUNTIME_FIELDS = (
    "terminal_supported",
    "expected_answer",
    "benchmark_correctness",
    "decisive",
    "evaluator_independence_label",
    "future_observations",
)


def load(path: str | Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def dump(path: str | Path, value: Any) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def evidence_metadata(task: Mapping[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(event["id"]): {
            "family": str(event["family"]),
            "kind": str(event["kind"]),
            "bears_on": str(event["bears_on"]),
            "depends_on": [str(v) for v in (event.get("depends_on") or [])],
            "revokes": [str(v) for v in (event.get("revokes") or [])],
        }
        for event in task["events"]
    }


def sanitized_row(event: Mapping[str, Any]) -> str:
    values = [
        str(event["step"]),
        str(event["id"]),
        str(event["family"]),
        str(event["kind"]),
        str(event["bears_on"]),
        ",".join(str(v) for v in (event.get("depends_on") or [])),
        ",".join(str(v) for v in (event.get("revokes") or [])),
    ]
    if any("\t" in value or "\n" in value for value in values):
        raise ValueError("runtime observation contains TSV delimiter")
    return "\t".join(values)


def null_state() -> dict[str, Any]:
    return {
        "has_answer": False,
        "operative_hypothesis": None,
        "committed_answer": None,
        "epistemic_status": "none",
        "confidence": None,
        "bundle_hash": None,
        "visited_states": None,
        "truncated": None,
        "evidence_edge_ids": [],
        "evidence_family_ids": [],
        "dependency_lineage_ids": [],
        "target_ranking": [],
    }


def _node_name(raw: Any, node_to_name: Mapping[int, str]) -> str | None:
    try:
        node = int(raw or 0)
    except (TypeError, ValueError):
        return None
    return node_to_name.get(node)


def _normalise_ranking(
    ranking: Any,
    node_to_name: Mapping[int, str],
) -> Any:
    if ranking is None:
        return None
    if not isinstance(ranking, list):
        return ranking
    out = []
    for item in ranking:
        if not isinstance(item, Mapping):
            out.append(item)
            continue
        row = dict(item)
        row["target_id"] = _node_name(row.get("target_id"), node_to_name)
        out.append(row)
    return out


def _normalise_state(
    state: Mapping[str, Any],
    node_to_name: Mapping[int, str],
) -> dict[str, Any]:
    row = dict(state)
    raw_operative = row.pop("operative_hypothesis_node", 0)
    raw_committed = row.pop("committed_answer_node", 0)
    has_answer = bool(row.get("has_answer"))
    status = str(row.get("epistemic_status") or "")
    row["operative_hypothesis"] = (
        _node_name(raw_operative, node_to_name) if has_answer else None
    )
    commits = status in {"resolved", "provisionally_resolved"} and has_answer
    row["committed_answer"] = (
        _node_name(raw_committed, node_to_name) if commits else None
    )
    row["target_ranking"] = _normalise_ranking(
        row.get("target_ranking"), node_to_name
    )
    return row


def _normalise_runtime_schema(episode: dict[str, Any]) -> None:
    hypothesis_nodes = episode.get("hypothesis_nodes") or {}
    node_to_name = {
        int(node): str(name)
        for name, node in hypothesis_nodes.items()
    }
    for step in episode.get("steps") or []:
        for key in ("previous_step_state", "initial_state", "final_state"):
            state = step.get(key)
            if isinstance(state, Mapping):
                step[key] = _normalise_state(state, node_to_name)
        for round_row in step.get("recovery_rounds") or []:
            if not isinstance(round_row, dict):
                continue
            round_row["target_ranking_before"] = _normalise_ranking(
                round_row.get("target_ranking_before"), node_to_name
            )
            round_row["target_ranking_after"] = _normalise_ranking(
                round_row.get("target_ranking_after"), node_to_name
            )


def c0_episode(task: Mapping[str, Any]) -> dict[str, Any]:
    metadata = evidence_metadata(task)
    previous = null_state()
    steps: list[dict[str, Any]] = []
    started = time.perf_counter()

    for event in task["events"]:
        decision = common_head([str(event["id"])], metadata)
        current = {
            "has_answer": decision["operative_hypothesis"] is not None,
            "operative_hypothesis": decision["operative_hypothesis"],
            "committed_answer": None,
            "epistemic_status": decision["status"],
            "confidence": None,
            "bundle_hash": None,
            "visited_states": None,
            "truncated": None,
            "evidence_edge_ids": [],
            "evidence_family_ids": sorted(
                set(decision["support_families"]["H1"])
                | set(decision["support_families"]["H2"])
                | set(decision["opposition_families"]["H1"])
                | set(decision["opposition_families"]["H2"])
            ),
            "dependency_lineage_ids": [],
            "target_ranking": [
                {"target_id": target, "net_family_score": score}
                for target, score in sorted(
                    decision["net_family_scores"].items(),
                    key=lambda item: (-item[1], item[0]),
                )
            ],
        }
        changed = (
            previous["operative_hypothesis"] != current["operative_hypothesis"]
            and int(event["step"]) > 1
        )
        events = []
        if changed:
            events.append(
                {
                    "type": "cross_step_belief_change",
                    "from": previous["operative_hypothesis"],
                    "to": current["operative_hypothesis"],
                    "triggering_evidence_ids": [str(event["id"])],
                    "step": int(event["step"]),
                }
            )
        steps.append(
            {
                "step": int(event["step"]),
                "ingested_evidence_ids": [str(event["id"])],
                "previous_step_state": previous,
                "initial_state": current,
                "recovery_rounds": [],
                "final_state": current,
                "native_events": events,
                "active_evidence_refs": [str(event["id"])],
                "common_head": decision,
                "telemetry_gaps": [],
                "execution": {
                    "execution_seconds": None,
                    "expansion_rounds": 0,
                    "visited_states_total": 0,
                    "evidence_edges_considered": 0,
                    "runtime_failure": False,
                },
            }
        )
        previous = current

    return {
        "episode_id": str(task["id"]),
        "configuration": "C0",
        "evidence_metadata": metadata,
        "steps": steps,
        "failures": [],
        "execution_seconds": time.perf_counter() - started,
    }


def runtime_episode(
    task: Mapping[str, Any],
    configuration: str,
    runtime_runner: Path,
    seed: int,
    work_root: Path,
) -> dict[str, Any]:
    underlying = "G0E" if configuration == "C1" else configuration
    safe_id = "".join(
        ch if ch.isalnum() or ch in "-_" else "_" for ch in str(task["id"])
    )
    source = work_root / f"{configuration}-{safe_id}.tsv"
    source.write_text(
        "\n".join(sanitized_row(event) for event in task["events"]) + "\n",
        encoding="utf-8",
    )
    db_dir = work_root / f"db-{configuration}-{safe_id}"
    started = time.perf_counter()
    proc = subprocess.run(
        [
            str(runtime_runner),
            underlying,
            str(task["id"]),
            str(source),
            str(db_dir),
            str(seed),
        ],
        capture_output=True,
        text=True,
    )
    elapsed = time.perf_counter() - started

    if proc.returncode != 0:
        return {
            "episode_id": str(task["id"]),
            "configuration": configuration,
            "evidence_metadata": evidence_metadata(task),
            "steps": [],
            "failures": [
                f"runtime exit={proc.returncode}",
                proc.stderr.strip(),
            ],
            "execution_seconds": elapsed,
        }

    try:
        episode = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        return {
            "episode_id": str(task["id"]),
            "configuration": configuration,
            "evidence_metadata": evidence_metadata(task),
            "steps": [],
            "failures": [f"malformed runtime JSON: {exc}", proc.stdout[-2000:]],
            "execution_seconds": elapsed,
        }

    episode["configuration"] = configuration
    episode["evidence_metadata"] = evidence_metadata(task)
    episode["execution_seconds"] = elapsed
    _normalise_runtime_schema(episode)

    if configuration == "C1":
        metadata = episode["evidence_metadata"]
        previous = null_state()
        for step in episode.get("steps", []):
            decision = common_head(step.get("active_evidence_refs") or [], metadata)
            current = dict(step["final_state"])
            current["has_answer"] = decision["operative_hypothesis"] is not None
            current["operative_hypothesis"] = decision["operative_hypothesis"]
            current["committed_answer"] = None
            current["epistemic_status"] = decision["status"]
            current["confidence"] = None
            current["target_ranking"] = [
                {"target_id": target, "net_family_score": score}
                for target, score in sorted(
                    decision["net_family_scores"].items(),
                    key=lambda item: (-item[1], item[0]),
                )
            ]
            current["evidence_family_ids"] = sorted(
                set(decision["support_families"]["H1"])
                | set(decision["support_families"]["H2"])
                | set(decision["opposition_families"]["H1"])
                | set(decision["opposition_families"]["H2"])
            )
            step["previous_step_state"] = previous
            step["initial_state"] = current
            step["final_state"] = current
            step["common_head"] = decision
            changed = (
                previous["operative_hypothesis"] != current["operative_hypothesis"]
                and int(step["step"]) > 1
            )
            step["native_events"] = [
                {
                    "type": "cross_step_belief_change",
                    "from": previous["operative_hypothesis"],
                    "to": current["operative_hypothesis"],
                    "triggering_evidence_ids": list(step["ingested_evidence_ids"]),
                    "step": int(step["step"]),
                }
            ] if changed else []
            previous = current

    return episode


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--runtime-runner", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--configuration", choices=CONFIGS, action="append")
    parser.add_argument("--seed", type=int, default=20261001)
    args = parser.parse_args()

    tasks = load(args.tasks)
    configs: Sequence[str] = tuple(args.configuration or CONFIGS)
    runtime_runner = Path(args.runtime_runner).resolve()
    if not runtime_runner.exists():
        raise SystemExit(f"runtime runner does not exist: {runtime_runner}")

    episodes: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="moolbase-v2-cycle4-") as td:
        work_root = Path(td)
        for config in configs:
            for task in tasks["episodes"]:
                if config == "C0":
                    episodes.append(c0_episode(task))
                else:
                    episodes.append(
                        runtime_episode(
                            task, config, runtime_runner, args.seed, work_root
                        )
                    )

    result = {
        "schema": "epistemic-process-v2-cycle4-unscored-v1",
        "protocol": str(tasks.get("protocol") or "epistemic-process-v2"),
        "mode": "unscored-mechanical-validation",
        "score_bearing": False,
        "score_bearing_authorized": False,
        "seed": args.seed,
        "configurations": list(configs),
        "runtime_input_fields": list(RUNTIME_FIELDS),
        "forbidden_runtime_fields": list(FORBIDDEN_RUNTIME_FIELDS),
        "episodes": episodes,
    }
    dump(args.out, result)

    failures = sum(bool(ep.get("failures")) for ep in episodes)
    print(
        json.dumps(
            {
                "episodes": len(episodes),
                "configurations": list(configs),
                "runtime_failures": failures,
                "score_bearing": False,
            },
            sort_keys=True,
        )
    )
    if failures:
        raise SystemExit("cycle4 runtime failures observed")


if __name__ == "__main__":
    main()
