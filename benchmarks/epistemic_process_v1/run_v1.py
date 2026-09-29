#!/usr/bin/env python3
"""Deterministic UNSCORED runner for Epistemic Process Evaluation v1.

This runner replays the preregistered event stream into four capability-bounded
configurations. It is deliberately not a score-bearing benchmark runner: its
purpose is to prove end-to-end plumbing, preserve configuration differences,
and expose adapter/runtime failures before the protocol is frozen.

No semantic event is inferred by the adapter. For G1/G2 this runner emits the
runtime event surface that adapt_v1.py is allowed to translate. B0/G0 expose
only direct decision projections.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List, Mapping, Sequence, Set

CONFIGS = ("B0", "G0", "G1", "G2")


def load(path: str | Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def dump(path: str | Path, value: Any) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _state(active: str | None, resolved: bool, abstain: bool) -> str:
    if abstain:
        return "abstain"
    if resolved:
        return "resolved"
    return "provisional" if active else "open"


def _independent_support(events: Sequence[Mapping[str, Any]], hypothesis: str) -> Set[str]:
    return {
        str(e["family"])
        for e in events
        if e.get("kind") == "support"
        and e.get("bears_on") == hypothesis
        and e.get("independent")
    }


def _is_refuted(events: Sequence[Mapping[str, Any]], hypothesis: str) -> bool:
    return any(
        e.get("bears_on") == hypothesis and e.get("kind") in {"refute", "revoke"}
        for e in events
    )


def _choose(events: Sequence[Mapping[str, Any]], initial: str, min_families: int, allow_revision: bool) -> tuple[str | None, bool, bool]:
    active = initial
    if allow_revision and _is_refuted(events, active):
        alternatives = [h for h in ("H1", "H2") if h != active]
        eligible = [h for h in alternatives if len(_independent_support(events, h)) >= min_families]
        if eligible:
            active = eligible[0]
        else:
            return None, False, True
    resolved = len(_independent_support(events, active)) >= min_families and not _is_refuted(events, active)
    return active, resolved, False


def _edge_id(step: int) -> int:
    return 1000 + step


def run_episode(task: Mapping[str, Any], configuration: str) -> Dict[str, Any]:
    if configuration not in CONFIGS:
        raise ValueError(configuration)
    initial = str(task["initial_preference"])
    min_families = int(task["minimum_independent_families_for_resolution"])
    hypothesis_nodes = {"H1": 101, "H2": 102}
    edge_map = {str(_edge_id(int(e["step"]))): str(e["id"]) for e in task["events"]}
    steps: List[Dict[str, Any]] = []
    seen: List[Mapping[str, Any]] = []
    previous = initial

    for event in task["events"]:
        seen.append(event)
        step_no = int(event["step"])
        evidence_edges = [_edge_id(int(e["step"])) for e in seen]

        # B0 is intentionally sticky/stateless with respect to contradiction.
        # G0 may use persistent independent evidence, but has no explicit
        # competing-hypothesis or challenge/reopen/revision machinery.
        allow_revision = configuration in {"G1", "G2"}
        active, resolved, abstain = _choose(seen, initial, min_families, allow_revision)
        status = _state(active, resolved, abstain)

        if configuration in {"B0", "G0"}:
            direct_active = initial
            direct_resolved = len(_independent_support(seen, direct_active)) >= min_families and not _is_refuted(seen, direct_active)
            steps.append({
                "step": step_no,
                "decision": {
                    "status": _state(direct_active, direct_resolved, False),
                    "hypothesis": direct_active,
                    "evidence_refs": [str(e["id"]) for e in seen],
                },
            })
            continue

        native: List[Dict[str, Any]] = [{
            "source": "hypokosh",
            "type": "hypothesis_set",
            "hypothesis_node": hypothesis_nodes.get(active or previous),
            "competing_hypotheses": [101, 102],
            "evidence_edges": evidence_edges,
            "epistemic_state": "contested" if not resolved else "resolved",
        }]

        decisive_against_previous = bool(event.get("decisive")) and event.get("bears_on") == previous
        if configuration == "G2" and decisive_against_previous:
            native.append({
                "source": "dwm",
                "type": "challenge",
                "hypothesis_node": hypothesis_nodes[previous],
                "competing_hypotheses": [101, 102],
                "reopen_nodes": [101, 102],
                "evidence_edges": [_edge_id(step_no)],
                "epistemic_state": "challenged",
            })
            native.append({
                "source": "dwm",
                "type": "reopen",
                "previous_hypothesis_node": hypothesis_nodes[previous],
                "hypothesis_node": hypothesis_nodes[previous],
                "competing_hypotheses": [101, 102],
                "reopen_nodes": [101, 102],
                "evidence_edges": [_edge_id(step_no)],
                "epistemic_state": "reopened",
            })

        if active and active != previous:
            native.append({
                "source": "hypokosh" if configuration == "G1" else "dwm",
                "type": "revision",
                "previous_hypothesis_node": hypothesis_nodes[previous],
                "hypothesis_node": hypothesis_nodes[active],
                "competing_hypotheses": [101, 102],
                "evidence_edges": evidence_edges,
                "epistemic_state": "revised",
            })
            previous = active

        native.append({
            "source": "graphene_core",
            "type": "terminal",
            "hypothesis_node": hypothesis_nodes.get(active),
            "competing_hypotheses": [101, 102],
            "evidence_edges": evidence_edges,
            "epistemic_state": status,
        })
        steps.append({"step": step_no, "native_events": native})

    return {
        "episode_id": task["id"],
        "configuration": configuration,
        "hypothesis_nodes": hypothesis_nodes,
        "edge_to_evidence_ref": edge_map,
        "steps": steps,
        "failures": [],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--configuration", choices=CONFIGS, action="append")
    parser.add_argument("--mode", choices=["unscored-dry-run"], default="unscored-dry-run")
    args = parser.parse_args()

    tasks = load(args.tasks)
    if tasks.get("reporting", {}).get("score_bearing_allowed"):
        raise SystemExit("runner is for the preregistered UNSCORED plumbing gate only")
    configs = tuple(args.configuration or CONFIGS)
    episodes = [run_episode(task, config) for config in configs for task in tasks["episodes"]]
    result = {
        "protocol": tasks["protocol"],
        "mode": args.mode,
        "runner": "deterministic-plumbing-v1",
        "score_bearing": False,
        "episodes": episodes,
    }
    dump(args.out, result)
    print(json.dumps({"episodes": len(episodes), "configurations": list(configs), "score_bearing": False}, sort_keys=True))


if __name__ == "__main__":
    main()
