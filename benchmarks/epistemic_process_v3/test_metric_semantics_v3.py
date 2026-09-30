#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SEM = json.loads((ROOT / "semantic_contract_v3.json").read_text(encoding="utf-8"))
METRICS = json.loads((ROOT / "metric_definitions_v3.json").read_text(encoding="utf-8"))


def outcome_metrics(rows):
    eligible = len(rows)
    commitments = [r for r in rows if r["committed"] is not None]
    correct_commitments = [
        r for r in rows
        if r["committed"] is not None and r["committed"] == r["expected"]
    ]
    false_commitments = [
        r for r in rows
        if r["committed"] is not None and r["committed"] != r["expected"]
    ]
    expected_abstain = [r for r in rows if r["expected"] is None]
    expected_commit = [r for r in rows if r["expected"] is not None]
    return {
        "commitment_coverage": len(commitments) / eligible,
        "correct_commitment_yield": len(correct_commitments) / eligible,
        "committed_accuracy": (
            None
            if not commitments
            else len(correct_commitments) / len(commitments)
        ),
        "false_commitment_incidence": len(false_commitments) / eligible,
        "appropriate_abstention_rate": (
            None
            if not expected_abstain
            else sum(r["committed"] is None for r in expected_abstain)
            / len(expected_abstain)
        ),
        "inappropriate_abstention_rate": (
            None
            if not expected_commit
            else sum(r["committed"] is None for r in expected_commit)
            / len(expected_commit)
        ),
    }


def canonical_family(raw):
    return raw if raw.startswith("family:") else f"family:{raw}"


def main():
    terms = SEM["terms"]
    assert terms["open"] != terms["abstention"]
    assert terms["contested"] != terms["abstention"]
    assert terms["operative_hypothesis"] != terms["committed_answer"]
    assert SEM["resolution"]["provisional_counts_as_resolved"] is False
    assert SEM["revision"]["challenge_alone"] is False
    assert SEM["revision"]["reopen_alone"] is False

    refuted = SEM["evidence_states"]["refuted_support"]
    revoked = SEM["evidence_states"]["revoked_evidence"]
    assert refuted["positive_support"] is True
    assert revoked["positive_support"] is False
    assert revoked["audit_visible"] is True

    exact_tie = SEM["tie"]["no_prior_material_opposition"]
    assert exact_tie["operative_hypothesis"] is None
    assert exact_tie["committed_answer"] is None

    perfect = outcome_metrics([
        {"expected": "H1", "committed": "H1"},
        {"expected": "H2", "committed": "H2"},
        {"expected": None, "committed": None},
        {"expected": None, "committed": None},
    ])
    always_abstain = outcome_metrics([
        {"expected": "H1", "committed": None},
        {"expected": "H2", "committed": None},
        {"expected": None, "committed": None},
        {"expected": None, "committed": None},
    ])
    always_answer = outcome_metrics([
        {"expected": "H1", "committed": "H1"},
        {"expected": "H2", "committed": "H2"},
        {"expected": None, "committed": "H1"},
        {"expected": None, "committed": "H2"},
    ])

    assert perfect["correct_commitment_yield"] == 0.5
    assert perfect["appropriate_abstention_rate"] == 1.0
    assert always_abstain["commitment_coverage"] == 0.0
    assert always_abstain["correct_commitment_yield"] == 0.0
    assert always_abstain["committed_accuracy"] is None
    assert always_abstain["inappropriate_abstention_rate"] == 1.0
    assert always_answer["commitment_coverage"] == 1.0
    assert always_answer["false_commitment_incidence"] == 0.5

    # Committed accuracy and correct-commitment yield are deliberately distinct.
    low_coverage = outcome_metrics([
        {"expected": "H1", "committed": "H1"},
        {"expected": "H2", "committed": None},
        {"expected": "H1", "committed": None},
        {"expected": "H2", "committed": None},
    ])
    assert low_coverage["committed_accuracy"] == 1.0
    assert low_coverage["correct_commitment_yield"] == 0.25

    assert canonical_family("ABC") == "family:ABC"
    assert canonical_family("family:ABC") == "family:ABC"
    assert METRICS["canonical_family_id"] == "family:<raw-id>"

    print("cycle2_metric_semantics=passed")


if __name__ == "__main__":
    main()
