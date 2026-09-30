#!/usr/bin/env python3
from __future__ import annotations

from claim_gate_v2 import evaluate_claim_gates


def binary(delta, low, high, p):
    return {
        "delta_treatment_minus_control": delta,
        "ci_low": low,
        "ci_high": high,
        "exact_mcnemar": {"p_value_two_sided_exact": p},
    }


def model():
    return {
        "claim_gates": {
            "graphene_persistence": {
                "minimum_accuracy_delta": 0.10,
                "minimum_evidence_retention_exactness": 0.99,
                "maximum_cross_episode_leakage_rate": 0.0,
                "maximum_single_step_control_regression": 0.05,
            },
            "dwm_process_value": {
                "minimum_challenge_precision": 0.80,
                "maximum_unnecessary_reopen_rate": 0.20,
                "minimum_reopen_usefulness_rate": 0.50,
                "maximum_false_commitment_regression": 0.05,
            },
            "dwm_outcome_value": {
                "minimum_outcome_delta": 0.10,
                "maximum_committed_accuracy_regression": 0.05,
            },
        }
    }


def fixture():
    return {
        "aggregate": {
            "C1": {
                "evidence_retention_exactness": 1.0,
                "cross_episode_leakage_rate": 0.0,
            },
            "G1": {
                "false_commitment_rate": 0.10,
                "committed_accuracy": 0.90,
            },
            "G2": {
                "false_commitment_rate": 0.05,
                "committed_accuracy": 0.90,
                "challenge_precision": 0.90,
                "unnecessary_reopen_rate": 0.10,
                "reopen_usefulness_rate": 0.60,
            },
        },
        "comparisons": {
            "C0->C1": {
                "operative_hypothesis_accuracy": binary(0.15, 0.08, 0.22, 0.01),
                "single_step_control_accuracy": binary(0.0, -0.03, 0.03, 1.0),
                "holm_adjusted_p_values": {
                    "operative_hypothesis_accuracy": 0.01
                },
            },
            "G1->G2": {
                "binary_outcomes": {
                    "operative_correct": binary(0.12, 0.04, 0.20, 0.01),
                    "terminal_commitment_correct": binary(0.08, -0.01, 0.17, 0.08),
                    "false_commitment": binary(-0.05, -0.10, 0.01, 0.12),
                },
                "holm_adjusted_p_values": {
                    "operative_correct": 0.03,
                    "terminal_commitment_correct": 0.16,
                    "false_commitment": 0.16,
                },
            },
        },
    }


def main():
    result = evaluate_claim_gates(fixture(), model())
    assert result["graphene_persistence"]["status"] == "EARNED"
    assert result["hypokosh_governed_selection"]["status"] == "INELIGIBLE_DESIGN"
    assert result["dwm_process_value"]["status"] == "EARNED"
    assert result["dwm_outcome_value"]["status"] == "EARNED"
    assert result["score_authorization"] is False

    bad = fixture()
    bad["comparisons"]["C0->C1"]["single_step_control_accuracy"] = binary(
        -0.10, -0.15, -0.05, 0.01
    )
    assert (
        evaluate_claim_gates(bad, model())["graphene_persistence"]["status"]
        == "NOT_EARNED"
    )

    bad = fixture()
    bad["aggregate"]["G2"]["challenge_precision"] = 0.5
    assert (
        evaluate_claim_gates(bad, model())["dwm_process_value"]["status"]
        == "NOT_EARNED"
    )

    bad = fixture()
    bad["comparisons"]["G1->G2"]["holm_adjusted_p_values"][
        "operative_correct"
    ] = 0.2
    assert (
        evaluate_claim_gates(bad, model())["dwm_outcome_value"]["status"]
        == "NOT_EARNED"
    )

    print("cycle6_claim_gate_contracts=passed")


if __name__ == "__main__":
    main()
