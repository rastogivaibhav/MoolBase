#!/usr/bin/env python3
"""Epistemic Process Evaluation v1.

The scorer is receipt-driven and implements the primary metrics preregistered
in PROTOCOL.md. It never calls the system under test and never repairs missing
runtime semantics. Adapter/runtime failures are preserved and excluded from
primary metric denominators according to metric_spec_v1.json.

Score-bearing mode is separately gated by score_gate_v1.py; until a matching
freeze manifest exists, this module remains usable only for UNSCORED dry runs.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Sequence

REQUIRED_RECEIPT_FIELDS = {
    "episode_id",
    "configuration",
    "decisions",
    "terminal_status",
    "terminal_hypothesis",
    "evidence_refs",
    "hypotheses",
    "transitions",
    "challenges",
    "evidence_metadata",
    "execution_receipts",
    "adapter_receipt",
}

RESPONSE_TYPES = {"downgrade", "reopen", "revise"}
SUPPORT_KIND = "support"
REFUTATION_KINDS = {"refute", "revoke"}


def load(path: str | Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def safe_rate(numerator: int | float, denominator: int | float) -> Optional[float]:
    if not denominator:
        return None
    return float(numerator) / float(denominator)


def _event_map(task: Mapping[str, Any]) -> Dict[str, Mapping[str, Any]]:
    return {str(event["id"]): event for event in task.get("events", [])}


def _bearing_ref(
    ref: str,
    hypothesis: Optional[str],
    events: Mapping[str, Mapping[str, Any]],
) -> bool:
    event = events.get(str(ref))
    return bool(
        event
        and hypothesis is not None
        and event.get("bears_on") == hypothesis
    )


def _evidence_use(
    receipt: Mapping[str, Any],
    events: Mapping[str, Mapping[str, Any]],
) -> Dict[str, Any]:
    applicable: List[Dict[str, Any]] = []

    for decision in receipt.get("decisions", []):
        hypothesis = decision.get("hypothesis")
        if hypothesis is None:
            continue
        refs = [str(value) for value in (decision.get("evidence_refs") or [])]
        applicable.append(
            {
                "kind": "decision",
                "step": int(decision.get("step", 0)),
                "hypothesis": hypothesis,
                "bearing": bool(refs)
                and any(_bearing_ref(ref, hypothesis, events) for ref in refs),
            }
        )

    for transition in receipt.get("transitions", []):
        if transition.get("type") != "revise":
            continue
        hypothesis = transition.get("to")
        if hypothesis is None:
            continue
        refs = [str(value) for value in (transition.get("evidence_refs") or [])]
        applicable.append(
            {
                "kind": "revision",
                "step": int(transition.get("step", 0)),
                "hypothesis": hypothesis,
                "bearing": bool(refs)
                and any(_bearing_ref(ref, hypothesis, events) for ref in refs),
            }
        )

    numerator = sum(1 for item in applicable if item["bearing"])
    denominator = len(applicable)
    return {
        "numerator": numerator,
        "denominator": denominator,
        "rate": safe_rate(numerator, denominator),
        "events": applicable,
    }


def _refutation_response_and_inertia(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    decisive = [
        event
        for event in task.get("events", [])
        if bool(event.get("decisive"))
        and str(event.get("kind")) in REFUTATION_KINDS
    ]
    decisions = receipt.get("decisions", [])
    transitions = receipt.get("transitions", [])
    details: List[Dict[str, Any]] = []

    for refutation in decisive:
        step = int(refutation["step"])
        contradicted = str(refutation["bears_on"])
        responses = sorted(
            int(transition.get("step", -1))
            for transition in transitions
            if int(transition.get("step", -1)) >= step
            and transition.get("type") in RESPONSE_TYPES
            and transition.get("from") == contradicted
        )
        response_step = responses[0] if responses else None

        later_operating_steps = [
            int(decision.get("step", 0))
            for decision in decisions
            if int(decision.get("step", 0)) > step
            and (response_step is None or int(decision.get("step", 0)) < response_step)
            and decision.get("hypothesis") == contradicted
        ]
        inertia = len(later_operating_steps)
        details.append(
            {
                "refutation_id": str(refutation["id"]),
                "refutation_step": step,
                "contradicted_hypothesis": contradicted,
                "responded": response_step is not None,
                "response_step": response_step,
                "inertia_steps": inertia,
                "operative_steps_after_refutation": later_operating_steps,
            }
        )

    responded = bool(details) and all(item["responded"] for item in details)
    inertia_values = [item["inertia_steps"] for item in details]
    return {
        "eligible": bool(details),
        "responded": responded,
        "inertia_steps": max(inertia_values) if inertia_values else None,
        "details": details,
    }


def _independent_families_until(
    task: Mapping[str, Any],
    hypothesis: Optional[str],
    step: int,
) -> set[str]:
    if hypothesis is None:
        return set()
    return {
        str(event["family"])
        for event in task.get("events", [])
        if int(event["step"]) <= step
        and event.get("bears_on") == hypothesis
        and event.get("kind") == SUPPORT_KIND
        and bool(event.get("independent"))
    }


def _material_refutation_until(
    task: Mapping[str, Any],
    hypothesis: Optional[str],
    step: int,
) -> bool:
    if hypothesis is None:
        return False
    return any(
        int(event["step"]) <= step
        and event.get("bears_on") == hypothesis
        and event.get("kind") in REFUTATION_KINDS
        and bool(event.get("decisive"))
        for event in task.get("events", [])
    )


def _false_convergence(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    threshold = int(task["minimum_independent_families_for_resolution"])
    violations: List[Dict[str, Any]] = []
    resolved_decisions = 0
    for decision in receipt.get("decisions", []):
        if decision.get("status") != "resolved":
            continue
        resolved_decisions += 1
        step = int(decision.get("step", 0))
        hypothesis = decision.get("hypothesis")
        families = _independent_families_until(task, hypothesis, step)
        contradicted = _material_refutation_until(task, hypothesis, step)
        if len(families) < threshold or contradicted:
            violations.append(
                {
                    "step": step,
                    "hypothesis": hypothesis,
                    "independent_families": sorted(families),
                    "required_families": threshold,
                    "materially_refuted": contradicted,
                }
            )
    return {
        "eligible": True,
        "false_convergence": bool(violations),
        "resolved_decisions": resolved_decisions,
        "violations": violations,
    }


def _independent_evidence_convergence(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
    events: Mapping[str, Mapping[str, Any]],
) -> Dict[str, Any]:
    terminal_hypothesis = receipt.get("terminal_hypothesis")
    final_decision = (receipt.get("decisions") or [{}])[-1]
    refs = [str(value) for value in (final_decision.get("evidence_refs") or [])]

    supporting_events = [
        events[ref]
        for ref in refs
        if ref in events
        and events[ref].get("kind") == SUPPORT_KIND
        and events[ref].get("bears_on") == terminal_hypothesis
    ]
    raw_supporting_items = len(supporting_events)
    independent_families = {
        str(event["family"])
        for event in supporting_events
        if bool(event.get("independent"))
    }
    all_support_families = {str(event["family"]) for event in supporting_events}
    correlated_duplicates = max(
        0, raw_supporting_items - len(independent_families)
    )
    threshold = int(task["minimum_independent_families_for_resolution"])
    terminal_resolved = receipt.get("terminal_status") == "resolved"
    earned_resolution = (
        terminal_resolved
        and len(independent_families) >= threshold
        and not _material_refutation_until(
            task,
            terminal_hypothesis,
            int(final_decision.get("step", 0)),
        )
    )
    return {
        "terminal_raw_supporting_items": raw_supporting_items,
        "terminal_support_families": sorted(all_support_families),
        "terminal_independent_families": sorted(independent_families),
        "terminal_independent_family_count": len(independent_families),
        "terminal_correlated_or_nonindependent_items": correlated_duplicates,
        "required_independent_families": threshold,
        "terminal_resolved": terminal_resolved,
        "earned_resolution": earned_resolution,
    }


def _selective_coverage(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    expected = task["terminal_supported"]
    terminal_status = receipt.get("terminal_status")
    terminal_hypothesis = receipt.get("terminal_hypothesis")
    abstained = terminal_status == "abstain"
    covered = not abstained and terminal_hypothesis is not None
    if expected == "ABSTAIN":
        terminal_correct = abstained
    else:
        terminal_correct = terminal_hypothesis == expected
    covered_correct = bool(covered and terminal_correct)
    return {
        "covered": covered,
        "abstained": abstained,
        "terminal_correct": terminal_correct,
        "covered_correct": covered_correct,
    }


def _receipt_completeness(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    configuration = str(receipt.get("configuration") or "")
    required = {
        "terminal_status",
        "evidence_references",
        "evidence_family_identity",
        "adapter_provenance",
    }
    if any(event.get("depends_on") for event in task.get("events", [])):
        required.add("dependency_identity")
    if any(
        bool(event.get("decisive"))
        and event.get("kind") in REFUTATION_KINDS
        for event in task.get("events", [])
    ):
        required.add("refutation_evidence")
    if configuration in {"G0", "G1", "G2"}:
        required.add("execution_provenance")
    if configuration in {"G1", "G2"}:
        required.add("active_or_competing_hypotheses")
    if receipt.get("challenges"):
        required.add("challenge_event")
    if any(t.get("type") == "reopen" for t in receipt.get("transitions", [])):
        required.add("reopen_transition")
    if any(t.get("type") == "revise" for t in receipt.get("transitions", [])):
        required.add("revision_transition")

    metadata = receipt.get("evidence_metadata") or {}
    refs = [str(value) for value in (receipt.get("evidence_refs") or [])]
    referenced_metadata = [
        metadata.get(ref) for ref in refs if isinstance(metadata.get(ref), Mapping)
    ]
    task_events = _event_map(task)

    present: set[str] = set()
    if receipt.get("terminal_status") is not None:
        present.add("terminal_status")
    if refs:
        present.add("evidence_references")
    if refs and all(
        isinstance(metadata.get(ref), Mapping)
        and str(metadata[ref].get("family") or "")
        for ref in refs
    ):
        present.add("evidence_family_identity")
    dependent_refs = [
        ref
        for ref in refs
        if ref in task_events and task_events[ref].get("depends_on")
    ]
    if not dependent_refs or all(
        isinstance(metadata.get(ref), Mapping)
        and metadata[ref].get("depends_on")
        for ref in dependent_refs
    ):
        present.add("dependency_identity")
    if receipt.get("adapter_receipt"):
        present.add("adapter_provenance")
    if receipt.get("execution_receipts"):
        present.add("execution_provenance")
    if receipt.get("hypotheses"):
        present.add("active_or_competing_hypotheses")

    decisive_ids = {
        str(event["id"])
        for event in task.get("events", [])
        if bool(event.get("decisive"))
        and event.get("kind") in REFUTATION_KINDS
    }
    if decisive_ids and decisive_ids.intersection(refs):
        present.add("refutation_evidence")
    if receipt.get("challenges"):
        present.add("challenge_event")
    if any(t.get("type") == "reopen" for t in receipt.get("transitions", [])):
        present.add("reopen_transition")
    revision_transitions = [
        t for t in receipt.get("transitions", []) if t.get("type") == "revise"
    ]
    if revision_transitions and all(
        t.get("from") is not None
        and t.get("to") is not None
        and t.get("evidence_refs")
        for t in revision_transitions
    ):
        present.add("revision_transition")

    applicable = sorted(required)
    present_required = sorted(required.intersection(present))
    missing = sorted(required - present)
    return {
        "required_fields": applicable,
        "applicable_fields": len(applicable),
        "present_fields": present_required,
        "present_field_count": len(present_required),
        "missing_fields": missing,
        "completeness_rate": safe_rate(len(present_required), len(applicable)),
        "complete": not missing,
    }


def _failure_row(
    task: Mapping[str, Any],
    *,
    missing_receipt: bool = False,
    malformed: bool = False,
    details: Sequence[str] = (),
    adapter_failure: bool = False,
) -> Dict[str, Any]:
    text = " ".join(str(value).lower() for value in details)
    failure_class = (
        "timeout"
        if "timeout" in text
        else "runtime_failure"
        if "runtime" in text or "exit=" in text or "crash" in text
        else "adapter_failure"
        if adapter_failure
        else "missing_receipt"
        if missing_receipt
        else "malformed_receipt"
    )
    return {
        "episode_id": task["id"],
        "malformed": malformed,
        "missing_receipt": missing_receipt,
        "adapter_failure": adapter_failure,
        "adapter_failure_detail": list(details),
        "failure_class": failure_class,
        "excluded_from_primary_metrics": True,
    }


def score_episode(
    task: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    missing = sorted(REQUIRED_RECEIPT_FIELDS - set(receipt))
    if missing:
        return _failure_row(
            task,
            malformed=True,
            details=["missing receipt fields: " + ", ".join(missing)],
        )
    if receipt.get("adapter_failure"):
        return _failure_row(
            task,
            adapter_failure=True,
            details=receipt.get("adapter_failure_detail", []),
        )

    events = _event_map(task)
    evidence_use = _evidence_use(receipt, events)
    response = _refutation_response_and_inertia(task, receipt)
    false_convergence = _false_convergence(task, receipt)
    independent = _independent_evidence_convergence(task, receipt, events)
    selective = _selective_coverage(task, receipt)
    completeness = _receipt_completeness(task, receipt)

    return {
        "episode_id": task["id"],
        "configuration": receipt["configuration"],
        "malformed": False,
        "missing_receipt": False,
        "adapter_failure": False,
        "failure_class": None,
        "excluded_from_primary_metrics": False,
        "evidence_use": evidence_use,
        "refutation_response": {
            "eligible": response["eligible"],
            "responded": response["responded"],
            "details": response["details"],
        },
        "revision_inertia_steps": response["inertia_steps"],
        "false_convergence": false_convergence,
        "independent_evidence_convergence": independent,
        "selective_coverage": selective,
        "receipt_completeness": completeness,
    }


def aggregate(rows: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
    valid = [
        row for row in rows if not row.get("excluded_from_primary_metrics", False)
    ]
    failures = [row for row in rows if row.get("excluded_from_primary_metrics")]

    evidence_num = sum(int(row["evidence_use"]["numerator"]) for row in valid)
    evidence_den = sum(int(row["evidence_use"]["denominator"]) for row in valid)

    ref_rows = [
        row for row in valid if row["refutation_response"]["eligible"]
    ]
    ref_num = sum(
        1 for row in ref_rows if row["refutation_response"]["responded"]
    )

    inertia_values = [
        int(row["revision_inertia_steps"])
        for row in ref_rows
        if row["revision_inertia_steps"] is not None
    ]

    false_rows = [
        row for row in valid if row["false_convergence"]["eligible"]
    ]
    false_num = sum(
        1 for row in false_rows if row["false_convergence"]["false_convergence"]
    )

    independent_raw = sum(
        int(row["independent_evidence_convergence"]["terminal_raw_supporting_items"])
        for row in valid
    )
    independent_families = sum(
        int(row["independent_evidence_convergence"]["terminal_independent_family_count"])
        for row in valid
    )
    correlated_items = sum(
        int(
            row["independent_evidence_convergence"][
                "terminal_correlated_or_nonindependent_items"
            ]
        )
        for row in valid
    )
    resolved = [
        row
        for row in valid
        if row["independent_evidence_convergence"]["terminal_resolved"]
    ]
    earned = sum(
        1
        for row in resolved
        if row["independent_evidence_convergence"]["earned_resolution"]
    )

    coverage_num = sum(
        1 for row in valid if row["selective_coverage"]["covered"]
    )
    abstain_num = sum(
        1 for row in valid if row["selective_coverage"]["abstained"]
    )
    covered_correct = sum(
        1 for row in valid if row["selective_coverage"]["covered_correct"]
    )
    overall_terminal_correct = sum(
        1 for row in valid if row["selective_coverage"]["terminal_correct"]
    )

    complete_num = sum(
        int(row["receipt_completeness"]["present_field_count"]) for row in valid
    )
    complete_den = sum(
        int(row["receipt_completeness"]["applicable_fields"]) for row in valid
    )
    fully_complete = sum(
        1 for row in valid if row["receipt_completeness"]["complete"]
    )

    return {
        "eligible_episodes": len(valid),
        "excluded_episodes": len(failures),
        "failure_classes": {
            name: sum(1 for row in failures if row.get("failure_class") == name)
            for name in (
                "adapter_failure",
                "runtime_failure",
                "timeout",
                "missing_receipt",
                "malformed_receipt",
            )
        },
        "evidence_use_rate": {
            "numerator": evidence_num,
            "denominator": evidence_den,
            "rate": safe_rate(evidence_num, evidence_den),
        },
        "refutation_response_rate": {
            "numerator": ref_num,
            "denominator": len(ref_rows),
            "rate": safe_rate(ref_num, len(ref_rows)),
        },
        "revision_inertia_steps": {
            "observations": len(inertia_values),
            "total": sum(inertia_values),
            "mean": safe_rate(sum(inertia_values), len(inertia_values)),
            "values": inertia_values,
            "direction": "lower_is_better",
        },
        "false_convergence_rate": {
            "numerator": false_num,
            "denominator": len(false_rows),
            "rate": safe_rate(false_num, len(false_rows)),
            "direction": "lower_is_better",
        },
        "independent_evidence_convergence": {
            "raw_supporting_items": independent_raw,
            "distinct_independent_families": independent_families,
            "correlated_or_nonindependent_items": correlated_items,
            "earned_resolutions": earned,
            "resolved_episodes": len(resolved),
            "earned_resolution_rate": safe_rate(earned, len(resolved)),
        },
        "selective_coverage": {
            "covered_episodes": coverage_num,
            "eligible_episodes": len(valid),
            "coverage_rate": safe_rate(coverage_num, len(valid)),
            "abstentions": abstain_num,
            "abstention_rate": safe_rate(abstain_num, len(valid)),
            "covered_correct": covered_correct,
            "covered_accuracy": safe_rate(covered_correct, coverage_num),
            "overall_terminal_correct": overall_terminal_correct,
            "overall_terminal_accuracy": safe_rate(
                overall_terminal_correct, len(valid)
            ),
        },
        "receipt_completeness": {
            "present_fields": complete_num,
            "applicable_fields": complete_den,
            "rate": safe_rate(complete_num, complete_den),
            "fully_complete_episodes": fully_complete,
            "eligible_episodes": len(valid),
            "fully_complete_rate": safe_rate(fully_complete, len(valid)),
        },
    }


def evaluate(
    tasks: Mapping[str, Any],
    receipts: Sequence[Mapping[str, Any]],
    *,
    mode: str,
) -> Dict[str, Any]:
    by_id = {receipt.get("episode_id"): receipt for receipt in receipts}
    rows: List[Dict[str, Any]] = []
    for task in tasks["episodes"]:
        receipt = by_id.get(task["id"])
        if receipt is None:
            rows.append(_failure_row(task, missing_receipt=True))
        else:
            rows.append(score_episode(task, receipt))
    return {
        "protocol": tasks["protocol"],
        "mode": mode,
        "score_bearing": mode == "score",
        "episodes": rows,
        "aggregate": aggregate(rows),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--receipts", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument(
        "--mode",
        choices=["unscored-dry-run", "score"],
        default="unscored-dry-run",
    )
    parser.add_argument(
        "--freeze-manifest",
        help="required in score mode; verified by score_gate_v1.py",
    )
    args = parser.parse_args()

    tasks = load(args.tasks)
    if args.mode == "score":
        if not args.freeze_manifest:
            raise SystemExit(
                "score-bearing execution blocked: freeze manifest is required"
            )
        from score_gate_v1 import verify_score_gate

        gate = verify_score_gate(Path(args.freeze_manifest))
        if not gate["valid"]:
            raise SystemExit(
                "score-bearing execution blocked: " + "; ".join(gate["errors"])
            )

    receipts = load(args.receipts)
    result = evaluate(tasks, receipts, mode=args.mode)
    target = Path(args.out)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
