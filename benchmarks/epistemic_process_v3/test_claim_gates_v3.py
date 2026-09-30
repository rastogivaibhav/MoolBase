#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
from pathlib import Path

from claim_gate_v3 import evaluate_claims

ROOT = Path(__file__).resolve().parent
GATES = json.loads((ROOT / "claim_gates_v3.json").read_text(encoding="utf-8"))


def good_summary():
    return {
        "tie_invariance_rate": 1.0,
        "exact_tie_unique_selection_rate": 0.0,
        "appropriate_abstention_rate_on_exact_ties": 1.0,
        "near_tie_correct_selection_rate": 1.0,
        "history_dependent_operative_accuracy_delta": 0.08,
        "history_dependent_delta_ci_low": 0.02,
        "history_dependent_Holm_p": 0.01,
        "single_step_control_accuracy_delta": 0.0,
        "evidence_retention_exactness": 1.0,
        "evidence_family_identity_exactness": 1.0,
        "dependency_lineage_exactness": 1.0,
        "cross_episode_leakage_rate": 0.0,
        "invalidated_active_support_leakage_rate": 0.0,
        "revocation_visibility_rate": 1.0,
        "supersession_visibility_rate": 1.0,
        "clear_non_tie_accuracy_delta": 0.0,
        "incumbent_replacement_policy_conformity": 1.0,
        "native_revision_rate": 0.90,
        "false_revision_rate": 0.0,
        "revision_latency_steps_p95": 1.0,
        "native_revision_receipt_completeness": 1.0,
        "challenge_precision": 0.95,
        "challenge_recall": 0.85,
        "unnecessary_challenge_rate": 0.05,
        "unnecessary_reopen_rate": 0.05,
        "reopen_opportunity_precision": 0.95,
        "exhausted_frontier_reopen_rate": 0.0,
        "reopen_usefulness_rate": 0.75,
        "evidence_discovery_after_reopen_rate": 0.65,
        "false_commitment_incidence_delta": -0.08,
        "false_commitment_incidence_delta_ci_high": -0.03,
        "false_commitment_incidence_Holm_p": 0.01,
        "G2_false_commitment_incidence": 0.02,
        "appropriate_abstention_delta": 0.02,
        "commitment_coverage_delta": -0.01,
        "correct_commitment_yield_delta": 0.04,
        "operative_hypothesis_accuracy_delta": 0.01,
        "committed_accuracy_delta": 0.01,
        "earned_resolution_rate": 0.80,
        "false_convergence_rate": 0.0,
    }


def status(summary, claim):
    return evaluate_claims(summary, GATES)["claims"][claim]["status"]


def main():
    perfect = good_summary()
    result = evaluate_claims(perfect, GATES)
    assert all(
        row["status"] == "EARNED"
        for row in result["claims"].values()
    )

    # Always-abstain can appear safe but destroys coverage/yield.
    always_abstain = good_summary()
    always_abstain.update({
        "false_commitment_incidence_delta": -0.10,
        "false_commitment_incidence_delta_ci_high": -0.05,
        "G2_false_commitment_incidence": 0.0,
        "commitment_coverage_delta": -1.0,
        "correct_commitment_yield_delta": -0.80,
    })
    assert status(always_abstain, "CLAIM-DWM-SAFETY") == "EARNED"
    assert status(always_abstain, "CLAIM-DWM-BROAD-OUTCOME") == "NOT_EARNED"

    # Always-answer unsafe behavior cannot earn safety.
    always_answer = good_summary()
    always_answer.update({
        "false_commitment_incidence_delta": 0.10,
        "false_commitment_incidence_delta_ci_high": 0.15,
        "false_commitment_incidence_Holm_p": 0.001,
        "G2_false_commitment_incidence": 0.20,
    })
    assert status(always_answer, "CLAIM-DWM-SAFETY") == "NOT_EARNED"
    assert status(always_answer, "CLAIM-DWM-BROAD-OUTCOME") == "NOT_EARNED"

    collapsed = good_summary()
    collapsed["commitment_coverage_delta"] = -0.20
    collapsed["correct_commitment_yield_delta"] = -0.15
    assert status(collapsed, "CLAIM-DWM-SAFETY") == "EARNED"
    assert status(collapsed, "CLAIM-DWM-BROAD-OUTCOME") == "NOT_EARNED"

    overactive = good_summary()
    overactive.update({
        "challenge_precision": 0.50,
        "unnecessary_challenge_rate": 0.50,
        "unnecessary_reopen_rate": 0.50,
        "reopen_usefulness_rate": 0.0,
        "evidence_discovery_after_reopen_rate": 0.0,
    })
    assert status(overactive, "CLAIM-DWM-PRECISION") == "NOT_EARNED"
    assert status(overactive, "CLAIM-DWM-USEFULNESS") == "NOT_EARNED"

    tie_biased = good_summary()
    tie_biased.update({
        "tie_invariance_rate": 0.5,
        "exact_tie_unique_selection_rate": 1.0,
        "appropriate_abstention_rate_on_exact_ties": 0.0,
    })
    assert status(tie_biased, "CLAIM-TIE-SYMMETRY") == "NOT_EARNED"

    symmetric_abstention = good_summary()
    assert status(symmetric_abstention, "CLAIM-TIE-SYMMETRY") == "EARNED"

    fabricated_revision = good_summary()
    fabricated_revision["native_revision_receipt_completeness"] = 0.0
    assert status(fabricated_revision, "CLAIM-REVISION") == "NOT_EARNED"

    false_resolution = good_summary()
    false_resolution.update({
        "earned_resolution_rate": 0.80,
        "false_convergence_rate": 0.20,
    })
    assert status(false_resolution, "CLAIM-EARNED-RESOLUTION") == "NOT_EARNED"

    ineligible = evaluate_claims(
        good_summary(),
        GATES,
        eligibility={"CLAIM-REVISION": False},
    )
    assert (
        ineligible["claims"]["CLAIM-REVISION"]["status"]
        == "INELIGIBLE_DESIGN"
    )

    print("cycle2_claim_gate_synthetics=passed cases=9")


if __name__ == "__main__":
    main()
