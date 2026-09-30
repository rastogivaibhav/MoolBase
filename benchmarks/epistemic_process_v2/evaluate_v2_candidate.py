#!/usr/bin/env python3
"""Fail-closed evaluator candidate for EP-PROCESS-V2.

Cycle 6 freezes evaluation semantics before any score-bearing execution.
This module must only be exercised on synthetic/adversarial fixtures until a
later immutable score gate explicitly authorizes the V2 score run.
"""
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence

from statistics_v2 import (
    BOOTSTRAP_RESAMPLES,
    BOOTSTRAP_SEED,
    holm_adjust,
    paired_binary_summary,
    paired_continuous_summary,
)

CONFIGS = ("C0", "G0E", "C1", "G1", "G2")
ANSWER_CONFIGS = {"C0", "C1", "G1", "G2"}
COMMITMENT_CONFIGS = {"G1", "G2"}
VALID_HYPOTHESES = {None, "H1", "H2"}
COMMITTING_STATUSES = {"resolved", "provisionally_resolved"}
VALID_STATUSES = {
    "none",
    "evidence_only",
    "operative_selected",
    "open",
    "resolved",
    "provisionally_resolved",
    "contested",
    "evidence_required",
    "abstain",
    "speculative",
}
FORBIDDEN_RUNTIME_KEYS = {
    "oracle",
    "terminal_operative",
    "terminal_committed",
    "response_window",
    "challenge_warranted_steps",
    "minimum_independent_families_for_resolution",
    "terminal_supported",
    "expected_answer",
    "benchmark_correctness",
    "decisive",
    "independent",
    "evaluator_independence_label",
    "future_observations",
    "task_family",
    "mirror_primary",
}


class EvaluationInputError(ValueError):
    pass


def rate(n: int, d: int) -> float | None:
    return None if d == 0 else n / d


def _task_event_map(task: Mapping[str, Any]) -> dict[str, Mapping[str, Any]]:
    return {str(event["id"]): event for event in task["events"]}


def _active_event_ids_by_step(task: Mapping[str, Any]) -> list[set[str]]:
    active: set[str] = set()
    snapshots: list[set[str]] = []
    for event in task["events"]:
        for revoked in event.get("revokes") or []:
            active.discard(str(revoked))
        active.add(str(event["id"]))
        snapshots.append(set(active))
    return snapshots


def _active_events_at(
    task: Mapping[str, Any],
    step: int,
) -> list[Mapping[str, Any]]:
    snapshots = _active_event_ids_by_step(task)
    if step < 1 or step > len(snapshots):
        raise EvaluationInputError(f"step out of range: {step}")
    ids = snapshots[step - 1]
    by_id = _task_event_map(task)
    return [by_id[event_id] for event_id in sorted(ids)]


def _independent_support_families(
    task: Mapping[str, Any],
    hypothesis: str | None,
    step: int,
) -> set[str]:
    if hypothesis is None:
        return set()
    return {
        str(event["family"])
        for event in _active_events_at(task, step)
        if event.get("bears_on") == hypothesis
        and event.get("kind") == "support"
        and bool(event.get("independent"))
    }


def _material_refutation_active(
    task: Mapping[str, Any],
    hypothesis: str | None,
    step: int,
) -> bool:
    if hypothesis is None:
        return False
    return any(
        event.get("bears_on") == hypothesis
        and event.get("kind") in {"refute", "revoke"}
        and bool(event.get("decisive"))
        for event in _active_events_at(task, step)
    )


def _iter_nested_keys(value: Any) -> Iterable[str]:
    if isinstance(value, Mapping):
        for key, item in value.items():
            yield str(key)
            yield from _iter_nested_keys(item)
    elif isinstance(value, list):
        for item in value:
            yield from _iter_nested_keys(item)


def _validate_state(
    state: Mapping[str, Any],
    where: str,
) -> None:
    operative = state.get("operative_hypothesis")
    committed = state.get("committed_answer")
    status = str(state.get("epistemic_status") or "")
    if operative not in VALID_HYPOTHESES:
        raise EvaluationInputError(f"{where}: invalid operative hypothesis")
    if committed not in VALID_HYPOTHESES:
        raise EvaluationInputError(f"{where}: invalid committed answer")
    if status not in VALID_STATUSES:
        raise EvaluationInputError(f"{where}: invalid epistemic status {status!r}")
    if committed is not None and status not in COMMITTING_STATUSES:
        raise EvaluationInputError(
            f"{where}: committed answer under noncommitting status {status}"
        )
    if committed is not None and operative != committed:
        raise EvaluationInputError(
            f"{where}: committed answer differs from operative hypothesis"
        )


def _validate_episode(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
    config: str,
) -> None:
    episode_id = str(task["id"])
    if episode.get("configuration") != config:
        raise EvaluationInputError(
            f"{episode_id}: configuration mixing detected"
        )
    if episode.get("failures"):
        raise EvaluationInputError(
            f"{config}/{episode_id}: runtime failure present"
        )

    expected_events = list(task["events"])
    steps = list(episode.get("steps") or [])
    if len(steps) != len(expected_events):
        raise EvaluationInputError(
            f"{config}/{episode_id}: expected {len(expected_events)} steps, "
            f"got {len(steps)}"
        )

    expected_metadata = {
        str(event["id"]): {
            "family": str(event["family"]),
            "kind": str(event["kind"]),
            "bears_on": str(event["bears_on"]),
            "depends_on": [str(x) for x in (event.get("depends_on") or [])],
            "revokes": [str(x) for x in (event.get("revokes") or [])],
        }
        for event in expected_events
    }
    raw_metadata = episode.get("evidence_metadata") or {}
    if raw_metadata != expected_metadata:
        raise EvaluationInputError(
            f"{config}/{episode_id}: evidence metadata differs from task contract"
        )

    allowed_refs = set(expected_metadata)
    previous_final: Mapping[str, Any] | None = None
    for index, (event, step) in enumerate(zip(expected_events, steps), start=1):
        if int(step.get("step", -1)) != index:
            raise EvaluationInputError(
                f"{config}/{episode_id}: noncanonical step numbering"
            )
        ingested = [str(x) for x in (step.get("ingested_evidence_ids") or [])]
        if ingested != [str(event["id"])]:
            raise EvaluationInputError(
                f"{config}/{episode_id}/step {index}: ingested evidence mismatch"
            )

        for state_name in ("previous_step_state", "initial_state", "final_state"):
            state = step.get(state_name)
            if not isinstance(state, Mapping):
                raise EvaluationInputError(
                    f"{config}/{episode_id}/step {index}: missing {state_name}"
                )
            _validate_state(
                state,
                f"{config}/{episode_id}/step {index}/{state_name}",
            )

        if previous_final is not None:
            previous = step["previous_step_state"]
            for field in (
                "operative_hypothesis",
                "committed_answer",
                "epistemic_status",
            ):
                if previous.get(field) != previous_final.get(field):
                    raise EvaluationInputError(
                        f"{config}/{episode_id}/step {index}: "
                        f"previous-state mismatch for {field}"
                    )
        previous_final = step["final_state"]

        active_refs = set(str(x) for x in (step.get("active_evidence_refs") or []))
        leaked = active_refs - allowed_refs
        if leaked:
            raise EvaluationInputError(
                f"{config}/{episode_id}: cross-episode evidence leakage "
                f"{sorted(leaked)}"
            )

        events = list(step.get("native_events") or [])
        event_types = [
            str(item.get("type") or "").lower()
            for item in events
            if isinstance(item, Mapping)
        ]
        prev_op = step["previous_step_state"].get("operative_hypothesis")
        initial_op = step["initial_state"].get("operative_hypothesis")
        expected_cross_step = index > 1 and prev_op != initial_op
        observed_cross_step = event_types.count("cross_step_belief_change")
        if observed_cross_step != int(expected_cross_step):
            raise EvaluationInputError(
                f"{config}/{episode_id}/step {index}: fabricated or missing "
                "cross-step belief-change event"
            )

        rounds = list(step.get("recovery_rounds") or [])
        changed_rounds = sum(
            bool(row.get("operative_hypothesis_changed"))
            for row in rounds
            if isinstance(row, Mapping)
        )
        revision_events = event_types.count("revision")
        if revision_events != changed_rounds:
            raise EvaluationInputError(
                f"{config}/{episode_id}/step {index}: revision event/round "
                "change mismatch"
            )
        if config == "G2":
            dwm_rounds = sum(
                row.get("trigger") == "dwm_opposition"
                for row in rounds
                if isinstance(row, Mapping)
            )
            if event_types.count("challenge") != dwm_rounds:
                raise EvaluationInputError(
                    f"{config}/{episode_id}/step {index}: challenge count "
                    "does not match DWM recovery rounds"
                )
            if event_types.count("reopen") != dwm_rounds:
                raise EvaluationInputError(
                    f"{config}/{episode_id}/step {index}: reopen count "
                    "does not match DWM recovery rounds"
                )


def validate_inputs(
    tasks: Mapping[str, Any],
    raw: Mapping[str, Any],
    *,
    expected_configs: Sequence[str] = CONFIGS,
) -> None:
    if raw.get("score_bearing") is not False:
        raise EvaluationInputError(
            "Cycle-6 evaluator validation accepts only unscored fixture input"
        )
    if raw.get("score_bearing_authorized") not in (False, None):
        raise EvaluationInputError("unexpected score authorization in fixture")

    forbidden_present = set(_iter_nested_keys(raw)) & FORBIDDEN_RUNTIME_KEYS
    if forbidden_present:
        raise EvaluationInputError(
            f"oracle/evaluator fields leaked into runtime evidence: "
            f"{sorted(forbidden_present)}"
        )

    configs = tuple(str(x) for x in (raw.get("configurations") or []))
    if len(configs) != len(set(configs)):
        raise EvaluationInputError("duplicate configuration declaration")
    if set(configs) != set(expected_configs):
        raise EvaluationInputError(
            f"configuration set mismatch expected={sorted(expected_configs)} "
            f"actual={sorted(configs)}"
        )

    task_list = list(tasks.get("episodes") or [])
    task_ids = [str(task["id"]) for task in task_list]
    if len(task_ids) != len(set(task_ids)):
        raise EvaluationInputError("duplicate task ids")
    tasks_by_id = {str(task["id"]): task for task in task_list}

    episodes = list(raw.get("episodes") or [])
    expected_pairs = {
        (task_id, config)
        for task_id in task_ids
        for config in expected_configs
    }
    observed: dict[tuple[str, str], Mapping[str, Any]] = {}
    for episode in episodes:
        pair = (
            str(episode.get("episode_id")),
            str(episode.get("configuration")),
        )
        if pair in observed:
            raise EvaluationInputError(f"duplicate episode/config pair {pair}")
        observed[pair] = episode

    observed_pairs = set(observed)
    if observed_pairs != expected_pairs:
        missing = sorted(expected_pairs - observed_pairs)
        extra = sorted(observed_pairs - expected_pairs)
        raise EvaluationInputError(
            f"incomplete/mixed evaluation matrix missing={missing[:5]} "
            f"extra={extra[:5]}"
        )

    for (task_id, config), episode in observed.items():
        _validate_episode(tasks_by_id[task_id], episode, config)


def _terminal_metrics(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
) -> dict[str, Any]:
    final = episode["steps"][-1]["final_state"]
    oracle = task["oracle"]
    operative_expected = oracle["terminal_operative"]
    committed_expected = oracle["terminal_committed"]
    operative_actual = final.get("operative_hypothesis")
    committed_actual = final.get("committed_answer")
    committed = committed_actual is not None
    return {
        "operative_correct": operative_actual == operative_expected,
        "terminal_commitment_correct": committed_actual == committed_expected,
        "committed": committed,
        "committed_correct": (
            committed and committed_actual == committed_expected
        ),
        "false_commitment": (
            committed and committed_actual != committed_expected
        ),
        "abstention_eligible": committed_expected is None,
        "appropriate_abstention": (
            committed_expected is None and committed_actual is None
        ),
    }


def _response_metric(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
    config: str,
) -> dict[str, Any] | None:
    window = task["oracle"].get("response_window")
    if not window:
        return None
    trigger = int(window["trigger_step"])
    end = trigger + int(window["max_latency_steps"])
    contradicted = window["contradicted"]
    acceptable = set(window["acceptable_to"])

    pre = episode["steps"][trigger - 2]["final_state"] if trigger > 1 else None
    pre_committed = None if pre is None else pre.get("committed_answer")

    for step in episode["steps"]:
        n = int(step["step"])
        if n < trigger or n > end:
            continue
        final = step["final_state"]
        operative = final.get("operative_hypothesis")
        committed = final.get("committed_answer")
        operative_response = operative in acceptable and operative != contradicted
        governed_downgrade = (
            config in COMMITMENT_CONFIGS
            and pre_committed == contradicted
            and committed is None
        )
        if operative_response or governed_downgrade:
            return {"responded": True, "latency_steps": n - trigger}
    return {"responded": False, "latency_steps": None}


def _persistence_metrics(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
) -> dict[str, int]:
    expected_snapshots = _active_event_ids_by_step(task)
    by_id = _task_event_map(task)
    exact = family_exact = derivation_exact = 0
    revocations = revocations_visible = 0

    for index, step in enumerate(episode["steps"]):
        expected_refs = expected_snapshots[index]
        actual_refs = set(str(x) for x in step.get("active_evidence_refs") or [])
        exact += actual_refs == expected_refs

        expected_families = {
            str(by_id[ref]["family"]) for ref in expected_refs
        }
        actual_families = set(
            str(x)
            for x in (step["final_state"].get("evidence_family_ids") or [])
        )
        family_exact += actual_families == expected_families

        expected_derivations = {
            ",".join(str(v) for v in (by_id[ref].get("depends_on") or []))
            for ref in expected_refs
            if by_id[ref].get("depends_on")
        }
        actual_derivations = set(
            str(x)
            for x in (step["final_state"].get("dependency_lineage_ids") or [])
        )
        derivation_exact += actual_derivations == expected_derivations

        event = task["events"][index]
        for revoked in event.get("revokes") or []:
            revocations += 1
            revocations_visible += str(revoked) not in actual_refs

    total = len(episode["steps"])
    return {
        "retention_exact": exact,
        "retention_total": total,
        "family_exact": family_exact,
        "family_total": total,
        "derivation_exact": derivation_exact,
        "derivation_total": total,
        "revocation_visible": revocations_visible,
        "revocation_total": revocations,
        "leakage_count": 0,
        "leakage_total": total,
    }


def _false_convergence_metrics(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
) -> dict[str, int]:
    threshold = int(task["minimum_independent_families_for_resolution"])
    resolved = violations = 0
    for step in episode["steps"]:
        final = step["final_state"]
        if final.get("epistemic_status") != "resolved":
            continue
        resolved += 1
        n = int(step["step"])
        hypothesis = final.get("operative_hypothesis")
        families = _independent_support_families(task, hypothesis, n)
        contradicted = _material_refutation_active(task, hypothesis, n)
        violations += (
            hypothesis is None
            or len(families) < threshold
            or contradicted
        )
    return {
        "resolved_decisions": resolved,
        "false_convergence_violations": violations,
    }


def _earned_resolution_metrics(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
) -> dict[str, int]:
    expected = task["oracle"]["terminal_committed"]
    if expected is None:
        return {"resolution_opportunity": 0, "earned_resolution": 0}
    terminal_step = len(task["events"])
    threshold = int(task["minimum_independent_families_for_resolution"])
    eligible = (
        len(_independent_support_families(task, expected, terminal_step))
        >= threshold
        and not _material_refutation_active(task, expected, terminal_step)
    )
    if not eligible:
        return {"resolution_opportunity": 0, "earned_resolution": 0}
    final = episode["steps"][-1]["final_state"]
    earned = (
        final.get("epistemic_status") == "resolved"
        and final.get("committed_answer") == expected
    )
    return {
        "resolution_opportunity": 1,
        "earned_resolution": int(earned),
    }


def _dwm_metrics(
    task: Mapping[str, Any],
    episode: Mapping[str, Any],
) -> dict[str, int]:
    warranted = set(int(x) for x in task["challenge_warranted_steps"])
    out = collections.Counter()
    for step in episode["steps"]:
        out["steps"] += 1
        step_no = int(step["step"])
        for event in step.get("native_events") or []:
            kind = str(event.get("type") or "").lower()
            if kind == "challenge":
                out["challenges"] += 1
                out["challenge_correct"] += step_no in warranted
            elif kind == "reopen":
                out["reopens"] += 1
                out["unnecessary_reopens"] += step_no not in warranted
            elif kind == "revision":
                out["within_call_revisions"] += 1

        for row in step.get("recovery_rounds") or []:
            if row.get("trigger") != "dwm_opposition":
                continue
            changed = any(
                bool(row.get(key))
                for key in (
                    "frontier_changed",
                    "rank_changed",
                    "operative_hypothesis_changed",
                    "status_changed",
                    "committed_answer_changed",
                )
            )
            out["useful_reopens"] += changed
            out["frontier_changes"] += bool(row.get("frontier_changed"))
            out["rank_changes"] += bool(row.get("rank_changed"))
            out["status_changes"] += bool(row.get("status_changed"))
    return dict(out)


def _cost_metrics(episode: Mapping[str, Any]) -> dict[str, float]:
    steps = list(episode["steps"])
    return {
        "execution_seconds": float(episode.get("execution_seconds") or 0.0),
        "visited_states": float(sum(
            int(step["execution"].get("visited_states_total") or 0)
            for step in steps
        )),
        "expansion_rounds": float(sum(
            int(step["execution"].get("expansion_rounds") or 0)
            for step in steps
        )),
        "evidence_edges_considered": float(sum(
            int(step["execution"].get("evidence_edges_considered") or 0)
            for step in steps
        )),
        "challenge_reopen_count": float(sum(
            str(event.get("type") or "").lower() in {"challenge", "reopen"}
            for step in steps
            for event in (step.get("native_events") or [])
        )),
    }


def evaluate(
    tasks: Mapping[str, Any],
    raw: Mapping[str, Any],
    *,
    expected_configs: Sequence[str] = CONFIGS,
) -> dict[str, Any]:
    validate_inputs(tasks, raw, expected_configs=expected_configs)
    tasks_by_id = {str(task["id"]): task for task in tasks["episodes"]}
    episodes_by_pair = {
        (str(ep["episode_id"]), str(ep["configuration"])): ep
        for ep in raw["episodes"]
    }

    aggregate: dict[str, Any] = {}
    per_episode: dict[str, dict[str, Any]] = {}

    for config in expected_configs:
        counts = collections.Counter()
        response_latencies: list[int] = []
        cost_rows: list[dict[str, float]] = []
        config_records: dict[str, Any] = {}

        for task_id in sorted(tasks_by_id):
            task = tasks_by_id[task_id]
            episode = episodes_by_pair[(task_id, config)]
            record: dict[str, Any] = {
                "episode_id": task_id,
                "task_family": str(task.get("task_family") or ""),
            }

            if config in ANSWER_CONFIGS:
                terminal = _terminal_metrics(task, episode)
                record.update(terminal)
                counts["operative_correct"] += terminal["operative_correct"]
                counts["operative_total"] += 1

                response = _response_metric(task, episode, config)
                if response is not None:
                    counts["response_total"] += 1
                    counts["response_count"] += response["responded"]
                    record["refutation_responded"] = response["responded"]
                    record["revision_latency_steps"] = response["latency_steps"]
                    if response["latency_steps"] is not None:
                        response_latencies.append(response["latency_steps"])
                else:
                    record["refutation_responded"] = None
                    record["revision_latency_steps"] = None

            if config in COMMITMENT_CONFIGS:
                terminal = record
                counts["commitment_episode_total"] += 1
                counts["committed_count"] += terminal["committed"]
                counts["committed_correct"] += terminal["committed_correct"]
                counts["false_commitment"] += terminal["false_commitment"]
                counts["abstention_eligible"] += terminal["abstention_eligible"]
                counts["appropriate_abstention"] += terminal[
                    "appropriate_abstention"
                ]

                false_conv = _false_convergence_metrics(task, episode)
                counts.update(false_conv)
                earned = _earned_resolution_metrics(task, episode)
                counts.update(earned)
                record["false_convergence"] = bool(
                    false_conv["false_convergence_violations"]
                )
                record["earned_resolution"] = bool(earned["earned_resolution"])
                record["resolution_opportunity"] = bool(
                    earned["resolution_opportunity"]
                )

            if config in {"G0E", "C1"}:
                persistence = _persistence_metrics(task, episode)
                counts.update(persistence)

            if config == "G2":
                counts.update(_dwm_metrics(task, episode))

            cost = _cost_metrics(episode)
            record["cost"] = cost
            cost_rows.append(cost)
            config_records[task_id] = record

        def cost_mean(name: str) -> float:
            return sum(row[name] for row in cost_rows) / len(cost_rows)

        aggregate[config] = {
            "operative_hypothesis_accuracy": rate(
                counts["operative_correct"], counts["operative_total"]
            ),
            "cross_step_refutation_response_rate": rate(
                counts["response_count"], counts["response_total"]
            ),
            "cross_step_revision_latency_steps_mean": (
                None if not response_latencies
                else sum(response_latencies) / len(response_latencies)
            ),
            "committed_coverage_rate": rate(
                counts["committed_count"], counts["commitment_episode_total"]
            ),
            "committed_accuracy": rate(
                counts["committed_correct"], counts["committed_count"]
            ),
            "false_commitment_rate": rate(
                counts["false_commitment"], counts["committed_count"]
            ),
            "appropriate_abstention_rate": rate(
                counts["appropriate_abstention"], counts["abstention_eligible"]
            ),
            "false_convergence_rate": rate(
                counts["false_convergence_violations"],
                counts["resolved_decisions"],
            ),
            "earned_resolution_rate": rate(
                counts["earned_resolution"], counts["resolution_opportunity"]
            ),
            "evidence_retention_exactness": rate(
                counts["retention_exact"], counts["retention_total"]
            ),
            "evidence_family_identity_exactness": rate(
                counts["family_exact"], counts["family_total"]
            ),
            "dependency_lineage_exactness": rate(
                counts["derivation_exact"], counts["derivation_total"]
            ),
            "revocation_visibility_rate": rate(
                counts["revocation_visible"], counts["revocation_total"]
            ),
            "cross_episode_leakage_rate": rate(
                counts["leakage_count"], counts["leakage_total"]
            ),
            "challenge_precision": rate(
                counts["challenge_correct"], counts["challenges"]
            ),
            "challenge_rate": rate(
                counts["challenges"], counts["steps"]
            ),
            "unnecessary_reopen_rate": rate(
                counts["unnecessary_reopens"], counts["reopens"]
            ),
            "reopen_usefulness_rate": rate(
                counts["useful_reopens"], counts["reopens"]
            ),
            "frontier_change_after_reopen_rate": rate(
                counts["frontier_changes"], counts["reopens"]
            ),
            "rank_change_after_reopen_rate": rate(
                counts["rank_changes"], counts["reopens"]
            ),
            "status_change_after_reopen_rate": rate(
                counts["status_changes"], counts["reopens"]
            ),
            "within_call_revision_rate": rate(
                counts["within_call_revisions"], counts["reopens"]
            ),
            "cost_means": {
                key: cost_mean(key)
                for key in (
                    "execution_seconds",
                    "visited_states",
                    "expansion_rounds",
                    "evidence_edges_considered",
                    "challenge_reopen_count",
                )
            },
            "counts": dict(sorted(counts.items())),
        }
        per_episode[config] = config_records

    comparisons = _comparison_statistics(per_episode, expected_configs)
    return {
        "aggregate": aggregate,
        "per_episode": per_episode,
        "comparisons": comparisons,
    }


def _paired_records(
    per_episode: Mapping[str, Mapping[str, Any]],
    control: str,
    treatment: str,
) -> list[tuple[Mapping[str, Any], Mapping[str, Any]]]:
    control_ids = set(per_episode[control])
    treatment_ids = set(per_episode[treatment])
    if control_ids != treatment_ids:
        raise EvaluationInputError(
            f"paired comparison {control}->{treatment} has mismatched ids"
        )
    return [
        (per_episode[control][task_id], per_episode[treatment][task_id])
        for task_id in sorted(control_ids)
    ]


def _comparison_statistics(
    per_episode: Mapping[str, Mapping[str, Any]],
    expected_configs: Sequence[str],
) -> dict[str, Any]:
    configs = set(expected_configs)
    out: dict[str, Any] = {}

    if {"C0", "C1"} <= configs:
        pairs = _paired_records(per_episode, "C0", "C1")
        binary = paired_binary_summary(
            [bool(c["operative_correct"]) for c, _ in pairs],
            [bool(t["operative_correct"]) for _, t in pairs],
        )
        control_pairs = [
            (c, t)
            for c, t in pairs
            if c.get("task_family") == "single_step_control"
        ]
        control_summary = None
        if control_pairs:
            control_summary = paired_binary_summary(
                [bool(c["operative_correct"]) for c, _ in control_pairs],
                [bool(t["operative_correct"]) for _, t in control_pairs],
            )
        out["C0->C1"] = {
            "operative_hypothesis_accuracy": binary,
            "single_step_control_accuracy": control_summary,
            "holm_adjusted_p_values": {
                "operative_hypothesis_accuracy": binary[
                    "exact_mcnemar"
                ]["p_value_two_sided_exact"]
            },
        }

    if {"C1", "G1"} <= configs:
        pairs = _paired_records(per_episode, "C1", "G1")
        binary = paired_binary_summary(
            [bool(c["operative_correct"]) for c, _ in pairs],
            [bool(t["operative_correct"]) for _, t in pairs],
        )
        out["C1->G1"] = {
            "operative_hypothesis_accuracy": binary,
            "commitment_causal_comparison_authorized": False,
            "reason": (
                "C1 emits no committed answer; commitment metrics cannot be "
                "causally compared against G1 without another governed control."
            ),
        }

    if {"G1", "G2"} <= configs:
        pairs = _paired_records(per_episode, "G1", "G2")
        binary_metrics = {}
        for name in (
            "operative_correct",
            "terminal_commitment_correct",
            "false_commitment",
        ):
            binary_metrics[name] = paired_binary_summary(
                [bool(c[name]) for c, _ in pairs],
                [bool(t[name]) for _, t in pairs],
            )
        p_values = {
            name: summary["exact_mcnemar"]["p_value_two_sided_exact"]
            for name, summary in binary_metrics.items()
        }
        costs = {}
        for name in (
            "execution_seconds",
            "visited_states",
            "expansion_rounds",
            "evidence_edges_considered",
            "challenge_reopen_count",
        ):
            costs[name] = paired_continuous_summary(
                [float(c["cost"][name]) for c, _ in pairs],
                [float(t["cost"][name]) for _, t in pairs],
            )
        out["G1->G2"] = {
            "binary_outcomes": binary_metrics,
            "holm_adjusted_p_values": holm_adjust(p_values),
            "costs": costs,
        }
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--raw", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument(
        "--mode",
        choices=("unscored-evaluator-test",),
        required=True,
    )
    args = parser.parse_args()

    tasks = json.loads(Path(args.tasks).read_text(encoding="utf-8"))
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))
    result = evaluate(tasks, raw)

    document = {
        "schema": "epistemic-process-v2-evaluator-candidate-v2",
        "mode": args.mode,
        "score_bearing": False,
        "bootstrap_seed": BOOTSTRAP_SEED,
        "bootstrap_resamples": BOOTSTRAP_RESAMPLES,
        **result,
    }
    Path(args.out).write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
