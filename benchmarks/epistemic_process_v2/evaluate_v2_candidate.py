#!/usr/bin/env python3
"""Candidate EP-PROCESS-V2 evaluator.

This module joins oracle-bearing tasks to runtime evidence. Cycle 5 only unit
tests this evaluator on synthetic fixtures; it MUST NOT be run against the
240-episode candidate execution before score authorization.
"""
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path
from typing import Any, Mapping


def rate(n: int, d: int):
    return None if d == 0 else n / d


def expected_active_refs(task: Mapping[str, Any]) -> list[set[str]]:
    active: set[str] = set()
    snapshots = []
    for event in task["events"]:
        for revoked in event.get("revokes") or []:
            active.discard(str(revoked))
        active.add(str(event["id"]))
        snapshots.append(set(active))
    return snapshots


def terminal_metrics(task: Mapping[str, Any], episode: Mapping[str, Any]):
    final = episode["steps"][-1]["final_state"]
    oracle = task["oracle"]
    operative_expected = oracle["terminal_operative"]
    committed_expected = oracle["terminal_committed"]
    operative_actual = final.get("operative_hypothesis")
    committed_actual = final.get("committed_answer")
    return {
        "operative_correct": operative_actual == operative_expected,
        "committed_correct": committed_actual == committed_expected,
        "expected_commitment": committed_expected,
        "actual_commitment": committed_actual,
        "false_commitment": (
            committed_actual is not None and
            committed_actual != committed_expected
        ),
        "appropriate_abstention": (
            committed_expected is None and committed_actual is None
        ),
    }


def response_metric(task: Mapping[str, Any], episode: Mapping[str, Any]):
    window = task["oracle"].get("response_window")
    if not window:
        return None
    trigger = int(window["trigger_step"])
    end = trigger + int(window["max_latency_steps"])
    contradicted = window["contradicted"]
    for step in episode["steps"]:
        n = int(step["step"])
        if n < trigger or n > end:
            continue
        final = step["final_state"]
        if (
            final.get("operative_hypothesis") != contradicted or
            final.get("committed_answer") is None
        ):
            return {"responded": True, "latency_steps": n - trigger}
    return {"responded": False, "latency_steps": None}


def persistence_metrics(task: Mapping[str, Any], episode: Mapping[str, Any]):
    expected = expected_active_refs(task)
    exact = 0
    family_exact = 0
    derivation_exact = 0
    revocations = 0
    revocations_visible = 0
    metadata = episode.get("evidence_metadata") or {}

    for index, step in enumerate(episode["steps"]):
        expected_refs = expected[index]
        actual_refs = set(step.get("active_evidence_refs") or [])
        exact += actual_refs == expected_refs

        expected_families = {
            str(metadata[ref]["family"])
            for ref in expected_refs
            if ref in metadata
        }
        actual_families = set(
            step["final_state"].get("evidence_family_ids") or []
        )
        family_exact += actual_families == expected_families

        expected_derivations = {
            ",".join(str(v) for v in (metadata[ref].get("depends_on") or []))
            for ref in expected_refs
            if ref in metadata and (metadata[ref].get("depends_on") or [])
        }
        actual_derivations = set(
            step["final_state"].get("dependency_lineage_ids") or []
        )
        derivation_exact += actual_derivations == expected_derivations

        event = task["events"][index]
        for revoked in event.get("revokes") or []:
            revocations += 1
            revocations_visible += str(revoked) not in actual_refs

    d = len(episode["steps"])
    return {
        "retention_exact": exact,
        "retention_total": d,
        "family_exact": family_exact,
        "family_total": d,
        "derivation_exact": derivation_exact,
        "derivation_total": d,
        "revocation_visible": revocations_visible,
        "revocation_total": revocations,
    }


def dwm_metrics(task: Mapping[str, Any], episode: Mapping[str, Any]):
    warranted = set(int(x) for x in task["challenge_warranted_steps"])
    challenges = challenge_correct = reopens = unnecessary = useful = 0
    frontier = rank = status = revisions = 0
    for step in episode["steps"]:
        step_no = int(step["step"])
        for event in step.get("native_events") or []:
            kind = str(event.get("type") or "").lower()
            if kind == "challenge":
                challenges += 1
                challenge_correct += step_no in warranted
            elif kind == "reopen":
                reopens += 1
                unnecessary += step_no not in warranted
            elif kind == "revision":
                revisions += 1
        for row in step.get("recovery_rounds") or []:
            if row.get("trigger") != "dwm_opposition":
                continue
            changed = any(bool(row.get(k)) for k in (
                "frontier_changed", "rank_changed",
                "operative_hypothesis_changed", "status_changed",
                "committed_answer_changed",
            ))
            useful += changed
            frontier += bool(row.get("frontier_changed"))
            rank += bool(row.get("rank_changed"))
            status += bool(row.get("status_changed"))
    return {
        "challenges": challenges,
        "challenge_correct": challenge_correct,
        "reopens": reopens,
        "unnecessary_reopens": unnecessary,
        "useful_reopens": useful,
        "frontier_changes": frontier,
        "rank_changes": rank,
        "status_changes": status,
        "within_call_revisions": revisions,
    }


def evaluate(tasks: Mapping[str, Any], raw: Mapping[str, Any]):
    tasks_by_id = {str(task["id"]): task for task in tasks["episodes"]}
    output = {}
    for config in raw["configurations"]:
        eps = [
            ep for ep in raw["episodes"]
            if ep["configuration"] == config
        ]
        agg = collections.Counter()
        latencies = []
        for ep in eps:
            task = tasks_by_id[str(ep["episode_id"])]
            if config != "G0E":
                tm = terminal_metrics(task, ep)
                agg["operative_correct"] += tm["operative_correct"]
                agg["operative_total"] += 1
                if config in {"G1", "G2"}:
                    agg["committed_correct"] += tm["committed_correct"]
                    agg["committed_total"] += 1
                    agg["false_commitment"] += tm["false_commitment"]
                    agg["appropriate_abstention"] += tm["appropriate_abstention"]
                    agg["abstention_eligible"] += tm["expected_commitment"] is None
                rm = response_metric(task, ep)
                if rm is not None:
                    agg["response_total"] += 1
                    agg["response_count"] += rm["responded"]
                    if rm["latency_steps"] is not None:
                        latencies.append(rm["latency_steps"])
            if config in {"G0E", "C1"}:
                pm = persistence_metrics(task, ep)
                agg.update(pm)
            if config == "G2":
                agg.update(dwm_metrics(task, ep))

        output[config] = {
            "operative_hypothesis_accuracy": rate(
                agg["operative_correct"], agg["operative_total"]),
            "cross_step_refutation_response_rate": rate(
                agg["response_count"], agg["response_total"]),
            "mean_response_latency_steps": (
                None if not latencies else sum(latencies) / len(latencies)
            ),
            "committed_accuracy": rate(
                agg["committed_correct"], agg["committed_total"]),
            "false_commitment_rate": rate(
                agg["false_commitment"], agg["committed_total"]),
            "appropriate_abstention_rate": rate(
                agg["appropriate_abstention"], agg["abstention_eligible"]),
            "evidence_retention_exactness": rate(
                agg["retention_exact"], agg["retention_total"]),
            "evidence_family_identity_exactness": rate(
                agg["family_exact"], agg["family_total"]),
            "dependency_lineage_exactness": rate(
                agg["derivation_exact"], agg["derivation_total"]),
            "revocation_visibility_rate": rate(
                agg["revocation_visible"], agg["revocation_total"]),
            "challenge_precision": rate(
                agg["challenge_correct"], agg["challenges"]),
            "unnecessary_reopen_rate": rate(
                agg["unnecessary_reopens"], agg["reopens"]),
            "reopen_usefulness_rate": rate(
                agg["useful_reopens"], agg["reopens"]),
            "frontier_change_after_reopen_rate": rate(
                agg["frontier_changes"], agg["reopens"]),
            "rank_change_after_reopen_rate": rate(
                agg["rank_changes"], agg["reopens"]),
            "status_change_after_reopen_rate": rate(
                agg["status_changes"], agg["reopens"]),
            "within_call_revision_rate": rate(
                agg["within_call_revisions"], agg["reopens"]),
            "counts": dict(agg),
        }
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--raw", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument(
        "--mode", choices=("unscored-evaluator-test",), required=True)
    args = parser.parse_args()
    tasks = json.loads(Path(args.tasks).read_text(encoding="utf-8"))
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))
    if raw.get("score_bearing") is not False:
        raise SystemExit("candidate evaluator refuses score-bearing input")
    result = {
        "schema": "epistemic-process-v2-evaluator-candidate-v1",
        "mode": args.mode,
        "score_bearing": False,
        "metrics": evaluate(tasks, raw),
    }
    Path(args.out).write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
