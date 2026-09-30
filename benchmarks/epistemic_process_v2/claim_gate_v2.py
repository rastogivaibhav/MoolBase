#!/usr/bin/env python3
"""Pre-score claim gate for EP-PROCESS-V2.

This module evaluates only thresholds frozen before score-bearing output exists.
It does not rank configurations and it cannot authorize a score run.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping


def _number(value: Any) -> float | None:
    if value is None:
        return None
    return float(value)


def _positive_effect(summary: Mapping[str, Any], minimum: float) -> bool:
    return (
        float(summary["delta_treatment_minus_control"]) >= minimum
        and float(summary["ci_low"]) > 0.0
    )


def _negative_effect(summary: Mapping[str, Any], minimum: float) -> bool:
    return (
        float(summary["delta_treatment_minus_control"]) <= -minimum
        and float(summary["ci_high"]) < 0.0
    )


def evaluate_claim_gates(
    evaluation: Mapping[str, Any],
    model: Mapping[str, Any],
) -> dict[str, Any]:
    aggregate = evaluation["aggregate"]
    comparisons = evaluation["comparisons"]
    gates = model["claim_gates"]

    persistence = gates["graphene_persistence"]
    c0c1 = comparisons["C0->C1"]
    persistence_effect = c0c1["operative_hypothesis_accuracy"]
    control = c0c1.get("single_step_control_accuracy")
    persistence_p = float(
        c0c1["holm_adjusted_p_values"]["operative_hypothesis_accuracy"]
    )
    control_delta = (
        None if control is None
        else float(control["delta_treatment_minus_control"])
    )
    persistence_checks = {
        "minimum_accuracy_delta": _positive_effect(
            persistence_effect,
            float(persistence["minimum_accuracy_delta"]),
        ),
        "holm_adjusted_p_le_0_05": persistence_p <= 0.05,
        "evidence_retention_exactness": (
            _number(aggregate["C1"]["evidence_retention_exactness"]) is not None
            and float(aggregate["C1"]["evidence_retention_exactness"])
            >= float(persistence["minimum_evidence_retention_exactness"])
        ),
        "cross_episode_leakage_zero": (
            _number(aggregate["C1"]["cross_episode_leakage_rate"]) == 0.0
        ),
        "single_step_control_present": control is not None,
        "single_step_control_no_material_regression": (
            control_delta is not None
            and control_delta
            >= -float(persistence["maximum_single_step_control_regression"])
        ),
    }

    process = gates["dwm_process_value"]
    g1_false = _number(aggregate["G1"]["false_commitment_rate"])
    g2_false = _number(aggregate["G2"]["false_commitment_rate"])
    false_regression_ok = (
        g1_false is not None
        and g2_false is not None
        and g2_false - g1_false
        <= float(process["maximum_false_commitment_regression"])
    )
    process_checks = {
        "challenge_precision": (
            _number(aggregate["G2"]["challenge_precision"]) is not None
            and float(aggregate["G2"]["challenge_precision"])
            >= float(process["minimum_challenge_precision"])
        ),
        "unnecessary_reopen_rate": (
            _number(aggregate["G2"]["unnecessary_reopen_rate"]) is not None
            and float(aggregate["G2"]["unnecessary_reopen_rate"])
            <= float(process["maximum_unnecessary_reopen_rate"])
        ),
        "reopen_usefulness_rate": (
            _number(aggregate["G2"]["reopen_usefulness_rate"]) is not None
            and float(aggregate["G2"]["reopen_usefulness_rate"])
            >= float(process["minimum_reopen_usefulness_rate"])
        ),
        "false_commitment_noninferiority": false_regression_ok,
    }

    outcome = gates["dwm_outcome_value"]
    g1g2 = comparisons["G1->G2"]
    binary = g1g2["binary_outcomes"]
    adjusted = g1g2["holm_adjusted_p_values"]
    min_delta = float(outcome["minimum_outcome_delta"])

    outcome_candidates = {
        "operative_correct": (
            _positive_effect(binary["operative_correct"], min_delta)
            and float(adjusted["operative_correct"]) <= 0.05
        ),
        "terminal_commitment_correct": (
            _positive_effect(binary["terminal_commitment_correct"], min_delta)
            and float(adjusted["terminal_commitment_correct"]) <= 0.05
        ),
        "false_commitment": (
            _negative_effect(binary["false_commitment"], min_delta)
            and float(adjusted["false_commitment"]) <= 0.05
        ),
    }
    g1_committed_accuracy = _number(aggregate["G1"]["committed_accuracy"])
    g2_committed_accuracy = _number(aggregate["G2"]["committed_accuracy"])
    committed_noninferiority = (
        g1_committed_accuracy is not None
        and g2_committed_accuracy is not None
        and g2_committed_accuracy - g1_committed_accuracy
        >= -float(outcome["maximum_committed_accuracy_regression"])
    )
    outcome_checks = {
        "at_least_one_preregistered_outcome_improves": any(
            outcome_candidates.values()
        ),
        "committed_accuracy_noninferiority": committed_noninferiority,
    }

    return {
        "graphene_persistence": {
            "status": (
                "EARNED" if all(persistence_checks.values())
                else "NOT_EARNED"
            ),
            "checks": persistence_checks,
        },
        "hypokosh_governed_selection": {
            "status": "INELIGIBLE_DESIGN",
            "checks": {
                "commitment_causal_comparison_authorized": False
            },
            "reason": (
                "C1 has no committed-answer capability; V2 can report the "
                "C1->G1 operative architectural delta but cannot claim a "
                "causal commitment-calibration advantage for HypoKosh."
            ),
        },
        "dwm_process_value": {
            "status": (
                "EARNED" if all(process_checks.values())
                else "NOT_EARNED"
            ),
            "checks": process_checks,
        },
        "dwm_outcome_value": {
            "status": (
                "EARNED" if all(outcome_checks.values())
                else "NOT_EARNED"
            ),
            "checks": outcome_checks,
            "candidate_endpoints": outcome_candidates,
        },
        "score_authorization": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evaluation", required=True)
    parser.add_argument("--measurement-model", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    evaluation = json.loads(
        Path(args.evaluation).read_text(encoding="utf-8")
    )
    model = json.loads(
        Path(args.measurement_model).read_text(encoding="utf-8")
    )
    result = evaluate_claim_gates(evaluation, model)
    Path(args.out).write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
