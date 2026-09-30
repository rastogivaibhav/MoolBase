#!/usr/bin/env python3
"""Deterministic post-score comparison for Epistemic Process v1.

This script does not decide which architecture is "best". It preserves the
frozen per-configuration metrics, computes transparent layer deltas, and reports
execution/search costs so behavioural gains are not conflated with free wins.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping

CONFIGS = ("B0", "G0", "G1", "G2")
LAYERS = (("B0", "G0"), ("G0", "G1"), ("G1", "G2"))


def load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def nested(mapping: Mapping[str, Any], *keys: str) -> Any:
    value: Any = mapping
    for key in keys:
        if not isinstance(value, Mapping):
            return None
        value = value.get(key)
    return value


METRICS = {
    "evidence_use_rate": ("evidence_use_rate", "rate"),
    "refutation_response_rate": ("refutation_response_rate", "rate"),
    "revision_inertia_mean": ("revision_inertia_steps", "mean"),
    "false_convergence_rate": ("false_convergence_rate", "rate"),
    "earned_resolution_rate": (
        "independent_evidence_convergence",
        "earned_resolution_rate",
    ),
    "coverage_rate": ("selective_coverage", "coverage_rate"),
    "abstention_rate": ("selective_coverage", "abstention_rate"),
    "covered_accuracy": ("selective_coverage", "covered_accuracy"),
    "overall_terminal_accuracy": (
        "selective_coverage",
        "overall_terminal_accuracy",
    ),
    "receipt_completeness_rate": ("receipt_completeness", "rate"),
}


def configuration_summary(
    raw: Mapping[str, Any],
    receipts: list[Mapping[str, Any]],
    evaluation: Mapping[str, Any],
) -> dict[str, Any]:
    aggregate = evaluation["aggregate"]
    metrics = {
        name: nested(aggregate, *path) for name, path in METRICS.items()
    }
    execution_times = [
        float(ep.get("execution_seconds", 0.0))
        for ep in raw.get("episodes", [])
        if ep.get("execution_seconds") is not None
    ]
    runtime_receipts = [
        item
        for ep in raw.get("episodes", [])
        for item in (ep.get("runtime_receipts") or [])
    ]
    reopens = sum(
        1
        for receipt in receipts
        for transition in (receipt.get("transitions") or [])
        if transition.get("type") == "reopen"
    )
    revisions = sum(
        1
        for receipt in receipts
        for transition in (receipt.get("transitions") or [])
        if transition.get("type") == "revise"
    )
    challenges = sum(len(receipt.get("challenges") or []) for receipt in receipts)
    return {
        "metrics": metrics,
        "cost": {
            "episodes": len(raw.get("episodes", [])),
            "execution_seconds_total": sum(execution_times),
            "execution_seconds_mean": (
                sum(execution_times) / len(execution_times)
                if execution_times
                else None
            ),
            "visited_states_total": sum(
                int(item.get("visited_states", 0)) for item in runtime_receipts
            ),
            "expansion_rounds_total": sum(
                int(item.get("expansion_rounds", 0)) for item in runtime_receipts
            ),
            "evidence_edge_count_total": sum(
                int(item.get("evidence_edge_count", 0)) for item in runtime_receipts
            ),
            "challenge_events": challenges,
            "reopen_transitions": reopens,
            "revision_transitions": revisions,
        },
        "failures": aggregate.get("failure_classes", {}),
        "eligible_episodes": aggregate.get("eligible_episodes"),
        "excluded_episodes": aggregate.get("excluded_episodes"),
    }


def delta(left: Any, right: Any) -> Any:
    if left is None or right is None:
        return None
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return float(right) - float(left)
    return None


def build_report(root: Path) -> dict[str, Any]:
    summaries: dict[str, Any] = {}
    evaluations: dict[str, Any] = {}
    receipts_by_config: dict[str, Any] = {}

    for config in CONFIGS:
        raw = load(root / f"raw-{config}.json")
        receipts = load(root / f"receipts-{config}.json")
        evaluation = load(root / f"scores-{config}.json")
        if raw.get("score_bearing") is not True:
            raise ValueError(f"{config} raw artifact is not score-bearing")
        if evaluation.get("score_bearing") is not True:
            raise ValueError(f"{config} evaluator artifact is not score-bearing")
        summaries[config] = configuration_summary(raw, receipts, evaluation)
        evaluations[config] = evaluation
        receipts_by_config[config] = {
            receipt.get("episode_id"): receipt for receipt in receipts
        }

    layer_effects: dict[str, Any] = {}
    for left, right in LAYERS:
        key = f"{left}->{right}"
        layer_effects[key] = {
            "metrics": {
                metric: delta(
                    summaries[left]["metrics"].get(metric),
                    summaries[right]["metrics"].get(metric),
                )
                for metric in METRICS
            },
            "cost": {
                field: delta(
                    summaries[left]["cost"].get(field),
                    summaries[right]["cost"].get(field),
                )
                for field in (
                    "execution_seconds_total",
                    "execution_seconds_mean",
                    "visited_states_total",
                    "expansion_rounds_total",
                    "evidence_edge_count_total",
                    "challenge_events",
                    "reopen_transitions",
                    "revision_transitions",
                )
            },
        }

    episode_ids = [
        row["episode_id"] for row in evaluations["B0"].get("episodes", [])
    ]
    per_episode: list[dict[str, Any]] = []
    for episode_id in episode_ids:
        row: dict[str, Any] = {"episode_id": episode_id, "configurations": {}}
        for config in CONFIGS:
            scored = next(
                item
                for item in evaluations[config]["episodes"]
                if item["episode_id"] == episode_id
            )
            receipt = receipts_by_config[config][episode_id]
            row["configurations"][config] = {
                "terminal_status": receipt.get("terminal_status"),
                "terminal_hypothesis": receipt.get("terminal_hypothesis"),
                "terminal_correct": nested(
                    scored, "selective_coverage", "terminal_correct"
                ),
                "covered": nested(scored, "selective_coverage", "covered"),
                "refutation_response": nested(
                    scored, "refutation_response", "responded"
                ),
                "revision_inertia_steps": scored.get("revision_inertia_steps"),
                "false_convergence": nested(
                    scored, "false_convergence", "false_convergence"
                ),
                "independent_family_count": nested(
                    scored,
                    "independent_evidence_convergence",
                    "terminal_independent_family_count",
                ),
                "receipt_completeness": nested(
                    scored, "receipt_completeness", "completeness_rate"
                ),
            }
        per_episode.append(row)

    return {
        "schema": "epistemic-process-v1-score-analysis",
        "configurations": summaries,
        "layer_effects": layer_effects,
        "per_episode": per_episode,
        "claim_interpretation": "not_performed_by_script",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    report = build_report(Path(args.artifacts_dir))
    target = Path(args.out)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
