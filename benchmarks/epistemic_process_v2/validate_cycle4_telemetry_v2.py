#!/usr/bin/env python3
"""Fail-closed mechanical validator for Cycle-4 V2 telemetry."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping

CONFIGS = {"C0", "G0E", "C1", "G1", "G2"}
FORBIDDEN = {
    "terminal_supported",
    "expected_answer",
    "benchmark_correctness",
    "decisive",
    "evaluator_independence_label",
    "future_observations",
}
NONCOMMITTING = {
    "open",
    "contested",
    "evidence_required",
    "speculative",
    "abstain",
    "evidence_only",
    "operative_selected",
}
REQUIRED_STEP_FIELDS = {
    "step",
    "ingested_evidence_ids",
    "previous_step_state",
    "initial_state",
    "recovery_rounds",
    "final_state",
    "native_events",
    "active_evidence_refs",
    "telemetry_gaps",
    "execution",
}
REQUIRED_STATE_FIELDS = {
    "has_answer",
    "operative_hypothesis",
    "committed_answer",
    "epistemic_status",
    "confidence",
    "bundle_hash",
    "visited_states",
    "truncated",
    "evidence_edge_ids",
    "evidence_family_ids",
    "dependency_lineage_ids",
    "target_ranking",
}


def load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_state(state: Mapping[str, Any], where: str) -> None:
    missing = REQUIRED_STATE_FIELDS - set(state)
    if missing:
        raise AssertionError(f"{where}: missing state fields {sorted(missing)}")
    status = str(state.get("epistemic_status") or "")
    if status in NONCOMMITTING and state.get("committed_answer") is not None:
        raise AssertionError(
            f"{where}: noncommitting status {status} carried committed_answer"
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw")
    args = parser.parse_args()
    raw = load(Path(args.raw))

    if raw.get("score_bearing") is not False:
        raise AssertionError("Cycle 4 must never be score-bearing")
    if raw.get("score_bearing_authorized") is not False:
        raise AssertionError("Cycle 4 score authorization must remain false")

    runtime_fields = set(raw.get("runtime_input_fields") or [])
    leaked = runtime_fields & FORBIDDEN
    if leaked:
        raise AssertionError(f"forbidden runtime fields declared: {sorted(leaked)}")

    seen_configs: set[str] = set()
    for episode in raw.get("episodes") or []:
        config = str(episode.get("configuration") or "")
        if config not in CONFIGS:
            raise AssertionError(f"unexpected configuration {config!r}")
        seen_configs.add(config)
        if episode.get("failures"):
            raise AssertionError(
                f"{config}/{episode.get('episode_id')}: runtime failure"
            )
        previous_final = None
        for step in episode.get("steps") or []:
            missing = REQUIRED_STEP_FIELDS - set(step)
            if missing:
                raise AssertionError(
                    f"{config}/{episode.get('episode_id')}/{step.get('step')}: "
                    f"missing step fields {sorted(missing)}"
                )
            validate_state(
                step["previous_step_state"],
                f"{config}/{episode.get('episode_id')}/{step['step']}/previous",
            )
            validate_state(
                step["initial_state"],
                f"{config}/{episode.get('episode_id')}/{step['step']}/initial",
            )
            validate_state(
                step["final_state"],
                f"{config}/{episode.get('episode_id')}/{step['step']}/final",
            )
            if previous_final is not None:
                prev = step["previous_step_state"]
                for field in (
                    "operative_hypothesis",
                    "committed_answer",
                    "epistemic_status",
                ):
                    if prev.get(field) != previous_final.get(field):
                        raise AssertionError(
                            f"{config}/{episode.get('episode_id')}/{step['step']}: "
                            f"previous state mismatch for {field}"
                        )
            previous_final = step["final_state"]

            if config == "G0E":
                if step["final_state"].get("operative_hypothesis") is not None:
                    raise AssertionError("G0E must not select a hypothesis")
                if step["final_state"].get("committed_answer") is not None:
                    raise AssertionError("G0E must not commit an answer")
            if config in {"C0", "C1"}:
                if step["final_state"].get("committed_answer") is not None:
                    raise AssertionError(
                        f"{config} common head must not emit committed answers"
                    )
                if step.get("common_head", {}).get("decision_head") != (
                    "epistemic-process-v2-common-head-v1"
                ):
                    raise AssertionError(
                        f"{config} did not use the frozen common decision head"
                    )

    if seen_configs != CONFIGS:
        raise AssertionError(
            f"missing configs: {sorted(CONFIGS - seen_configs)}"
        )

    print(
        json.dumps(
            {
                "cycle4_telemetry_contract": "passed",
                "configurations": sorted(seen_configs),
                "score_bearing": False,
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
