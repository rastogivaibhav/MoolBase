#!/usr/bin/env python3
"""Epistemic Process v1 runtime-receipt adapter.

This module translates runtime-observable state into the common receipt schema.
It must not infer semantic events that the runtime did not expose.

Raw input contract
------------------
{
  "protocol": "epistemic-process-v1",
  "mode": "unscored-dry-run",
  "episodes": [
    {
      "episode_id": "...",
      "configuration": "B0|G0|G1|G2",
      "hypothesis_nodes": {"H1": 1, "H2": 2},
      "edge_to_evidence_ref": {"10": "e1", "11": "e2"},
      "steps": [
        {
          "step": 1,
          "decision": { ... },              # B0/G0 direct native projection
          "native_events": [ ... ]          # G1/G2 NativeEpistemicEvent JSON
        }
      ],
      "failures": []
    }
  ]
}

For B0/G0, a runner may provide a direct decision projection because those
configurations do not expose HypoKosh/DWM native events. For G1/G2, decisions
and transitions are translated from NativeEpistemicEvent records only.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

CONFIGS = {"B0", "G0", "G1", "G2"}
DWM_EVENT_TYPES = {"challenge", "reopen"}
TRANSITION_EVENT_TYPES = {"reopen": "reopen", "revision": "revise"}

STATUS_MAP = {
    "resolved": "resolved",
    "provisionally_resolved": "provisional",
    "provisional": "provisional",
    "contested": "open",
    "evidence_required": "open",
    "speculative": "open",
    "selected": "provisional",
    "revised": "provisional",
    "challenged": "open",
    "reopened": "open",
    "abstain": "abstain",
    "open": "open",
}


class AdapterError(RuntimeError):
    pass


def load(path: str | Path) -> Any:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def dump(path: str | Path, value: Any) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def _int_key_map(mapping: Mapping[str, Any]) -> Dict[int, Any]:
    out: Dict[int, Any] = {}
    for key, value in mapping.items():
        try:
            parsed = int(key)
        except (TypeError, ValueError) as exc:
            raise AdapterError(f"non-integer runtime id in mapping: {key!r}") from exc
        out[parsed] = value
    return out


def _node_to_hypothesis(
    node_id: Any,
    node_to_hypothesis: Mapping[int, str],
) -> Optional[str]:
    # Graphene node id 0 is valid. Only an explicit JSON/Python null means
    # "no hypothesis"; never reserve an integer node id as a sentinel.
    if node_id is None:
        return None
    try:
        parsed = int(node_id)
    except (TypeError, ValueError) as exc:
        raise AdapterError(f"invalid hypothesis node id: {node_id!r}") from exc
    if parsed not in node_to_hypothesis:
        raise AdapterError(f"unknown hypothesis node id: {parsed}")
    return node_to_hypothesis[parsed]


def _edge_refs(
    edge_ids: Iterable[Any],
    edge_to_ref: Mapping[int, str],
) -> List[str]:
    refs: List[str] = []
    for raw in edge_ids or []:
        try:
            edge_id = int(raw)
        except (TypeError, ValueError) as exc:
            raise AdapterError(f"invalid runtime evidence edge id: {raw!r}") from exc
        if edge_id not in edge_to_ref:
            raise AdapterError(f"unknown runtime evidence edge id: {edge_id}")
        refs.append(edge_to_ref[edge_id])
    return sorted(set(refs))


def _normalise_status(value: Any) -> str:
    status = str(value or "open").strip().lower()
    if status not in STATUS_MAP:
        raise AdapterError(f"unsupported runtime epistemic state: {status!r}")
    return STATUS_MAP[status]


def _event_source(event: Mapping[str, Any]) -> str:
    return str(event.get("source") or "").strip().lower()


def _event_type(event: Mapping[str, Any]) -> str:
    return str(event.get("type") or "").strip().lower()


def _validate_capability_boundary(configuration: str, event: Mapping[str, Any]) -> None:
    source = _event_source(event)
    kind = _event_type(event)

    if configuration in {"B0", "G0"}:
        raise AdapterError(
            f"{configuration} must not provide HypoKosh/DWM native events; "
            "use direct runtime decision projection"
        )
    if configuration == "G1" and (source == "dwm" or kind in DWM_EVENT_TYPES):
        raise AdapterError(
            "G1 observed a DWM semantic event even though DWM is disabled"
        )
    if configuration == "G2" and kind in DWM_EVENT_TYPES and source != "dwm":
        raise AdapterError(
            f"G2 {kind!r} event must be emitted by source='dwm', got {source!r}"
        )


def _direct_decision(
    step: Mapping[str, Any],
    configuration: str,
) -> Dict[str, Any]:
    decision = step.get("decision")
    if not isinstance(decision, Mapping):
        raise AdapterError(
            f"{configuration} step {step.get('step')} lacks direct runtime decision"
        )
    hypothesis = decision.get("hypothesis")
    if hypothesis is not None and hypothesis not in {"H1", "H2"}:
        raise AdapterError(f"unsupported direct hypothesis id: {hypothesis!r}")
    refs = sorted(set(decision.get("evidence_refs") or []))
    return {
        "step": int(step["step"]),
        "status": _normalise_status(decision.get("status")),
        "hypothesis": hypothesis,
        "evidence_refs": refs,
    }


def _terminal_event(events: Sequence[Mapping[str, Any]]) -> Mapping[str, Any]:
    terminals = [e for e in events if _event_type(e) == "terminal"]
    if not terminals:
        raise AdapterError("G1/G2 runtime step has no native terminal event")
    if len(terminals) != 1:
        raise AdapterError(
            f"G1/G2 runtime step has {len(terminals)} terminal events; expected exactly one"
        )
    return terminals[0]


def _native_decision(
    step: Mapping[str, Any],
    configuration: str,
    node_to_hypothesis: Mapping[int, str],
    edge_to_ref: Mapping[int, str],
) -> Dict[str, Any]:
    events = step.get("native_events")
    if not isinstance(events, list):
        raise AdapterError(
            f"{configuration} step {step.get('step')} lacks native_events"
        )
    for event in events:
        if not isinstance(event, Mapping):
            raise AdapterError("native event is not an object")
        _validate_capability_boundary(configuration, event)

    terminal = _terminal_event(events)
    return {
        "step": int(step["step"]),
        "status": _normalise_status(terminal.get("epistemic_state")),
        "hypothesis": _node_to_hypothesis(
            terminal.get("hypothesis_node"), node_to_hypothesis
        ),
        "evidence_refs": _edge_refs(
            terminal.get("evidence_edges") or [], edge_to_ref
        ),
    }


def _transitions_from_native_events(
    step: Mapping[str, Any],
    configuration: str,
    node_to_hypothesis: Mapping[int, str],
    edge_to_ref: Mapping[int, str],
) -> List[Dict[str, Any]]:
    transitions: List[Dict[str, Any]] = []
    events = step.get("native_events") or []
    for event in events:
        _validate_capability_boundary(configuration, event)
        kind = _event_type(event)
        if kind not in TRANSITION_EVENT_TYPES:
            continue
        previous = _node_to_hypothesis(
            event.get("previous_hypothesis_node"),
            node_to_hypothesis,
        )
        current = _node_to_hypothesis(
            event.get("hypothesis_node"),
            node_to_hypothesis,
        )
        transitions.append(
            {
                "step": int(step["step"]),
                "type": TRANSITION_EVENT_TYPES[kind],
                "from": previous,
                "to": current,
                "evidence_refs": _edge_refs(
                    event.get("evidence_edges") or [], edge_to_ref
                ),
            }
        )
    return transitions


def _challenges_from_native_events(
    step: Mapping[str, Any],
    configuration: str,
    node_to_hypothesis: Mapping[int, str],
    edge_to_ref: Mapping[int, str],
) -> List[Dict[str, Any]]:
    challenges: List[Dict[str, Any]] = []
    for event in step.get("native_events") or []:
        _validate_capability_boundary(configuration, event)
        if _event_type(event) != "challenge":
            continue
        challenges.append(
            {
                "step": int(step["step"]),
                "hypothesis": _node_to_hypothesis(
                    event.get("hypothesis_node"), node_to_hypothesis
                ),
                "competing_hypotheses": [
                    hypothesis
                    for hypothesis in (
                        _node_to_hypothesis(raw, node_to_hypothesis)
                        for raw in event.get("competing_hypotheses") or []
                    )
                    if hypothesis is not None
                ],
                "reopen_nodes": [
                    hypothesis
                    for hypothesis in (
                        _node_to_hypothesis(raw, node_to_hypothesis)
                        for raw in event.get("reopen_nodes") or []
                    )
                    if hypothesis is not None
                ],
                "evidence_refs": _edge_refs(
                    event.get("evidence_edges") or [], edge_to_ref
                ),
            }
        )
    return challenges


def _native_hypotheses(
    steps: Sequence[Mapping[str, Any]],
    configuration: str,
    node_to_hypothesis: Mapping[int, str],
) -> List[Dict[str, str]]:
    if configuration in {"B0", "G0"}:
        return []

    seen: Dict[str, str] = {}
    for step in steps:
        for event in step.get("native_events") or []:
            _validate_capability_boundary(configuration, event)
            kind = _event_type(event)
            for raw_node in event.get("competing_hypotheses") or []:
                hypothesis = _node_to_hypothesis(raw_node, node_to_hypothesis)
                if hypothesis:
                    seen.setdefault(hypothesis, "active")
            if kind == "revision":
                previous = _node_to_hypothesis(
                    event.get("previous_hypothesis_node"), node_to_hypothesis
                )
                current = _node_to_hypothesis(
                    event.get("hypothesis_node"), node_to_hypothesis
                )
                if previous:
                    seen[previous] = "downgraded"
                if current:
                    seen[current] = "active"
            elif kind == "terminal":
                current = _node_to_hypothesis(
                    event.get("hypothesis_node"), node_to_hypothesis
                )
                state = _normalise_status(event.get("epistemic_state"))
                if current and state == "resolved":
                    seen[current] = "resolved"

    return [{"id": key, "status": seen[key]} for key in sorted(seen)]


def _failure_receipt(
    episode: Mapping[str, Any],
    errors: Sequence[str],
    observed_events: int,
) -> Dict[str, Any]:
    configuration = str(episode.get("configuration") or "")
    return {
        "episode_id": episode.get("episode_id"),
        "configuration": configuration,
        "decisions": [],
        "terminal_status": "open",
        "terminal_hypothesis": None,
        "evidence_refs": [],
        "hypotheses": [],
        "transitions": [],
        "challenges": [],
        "evidence_metadata": {},
        "execution_receipts": [],
        "adapter_failure": True,
        "adapter_failure_detail": list(errors),
        "adapter_receipt": {
            "adapter": configuration,
            "runtime_events_observed": observed_events,
            "synthetic_semantic_events": 0,
            "notes": list(errors),
        },
    }


def adapt_episode(episode: Mapping[str, Any]) -> Dict[str, Any]:
    configuration = str(episode.get("configuration") or "")
    if configuration not in CONFIGS:
        return _failure_receipt(
            episode,
            [f"unsupported configuration: {configuration!r}"],
            0,
        )

    steps = episode.get("steps")
    if not isinstance(steps, list) or not steps:
        return _failure_receipt(episode, ["episode has no runtime steps"], 0)

    observed_events = sum(
        len(step.get("native_events") or [])
        for step in steps
        if isinstance(step, Mapping)
    )

    explicit_failures = [
        str(value) for value in (episode.get("failures") or []) if str(value)
    ]
    if explicit_failures:
        return _failure_receipt(episode, explicit_failures, observed_events)

    try:
        hypothesis_nodes = episode.get("hypothesis_nodes") or {}
        node_to_hypothesis = {
            int(node): hypothesis for hypothesis, node in hypothesis_nodes.items()
        }
        edge_to_ref = _int_key_map(episode.get("edge_to_evidence_ref") or {})
        raw_evidence_metadata = episode.get("evidence_metadata") or {}
        if not isinstance(raw_evidence_metadata, Mapping):
            raise AdapterError("evidence_metadata is not an object")
        evidence_metadata = {
            str(ref): {
                "family": str(metadata.get("family") or ""),
                "kind": str(metadata.get("kind") or ""),
                "bears_on": str(metadata.get("bears_on") or ""),
                "depends_on": sorted(
                    str(value) for value in (metadata.get("depends_on") or [])
                ),
            }
            for ref, metadata in raw_evidence_metadata.items()
            if isinstance(metadata, Mapping)
        }

        decisions: List[Dict[str, Any]] = []
        transitions: List[Dict[str, Any]] = []
        challenges: List[Dict[str, Any]] = []
        evidence_refs: List[str] = []

        previous_step = 0
        for step in steps:
            if not isinstance(step, Mapping):
                raise AdapterError("runtime step is not an object")
            step_number = int(step.get("step", 0))
            if step_number <= previous_step:
                raise AdapterError("runtime steps are not strictly increasing")
            previous_step = step_number

            if configuration in {"B0", "G0"}:
                decision = _direct_decision(step, configuration)
            else:
                decision = _native_decision(
                    step,
                    configuration,
                    node_to_hypothesis,
                    edge_to_ref,
                )
                transitions.extend(
                    _transitions_from_native_events(
                        step,
                        configuration,
                        node_to_hypothesis,
                        edge_to_ref,
                    )
                )
                challenges.extend(
                    _challenges_from_native_events(
                        step,
                        configuration,
                        node_to_hypothesis,
                        edge_to_ref,
                    )
                )
            decisions.append(decision)
            evidence_refs.extend(decision["evidence_refs"])

        terminal = decisions[-1]
        hypotheses = _native_hypotheses(
            steps, configuration, node_to_hypothesis
        )
        if configuration in {"B0", "G0"}:
            # Do not manufacture competing-hypothesis state for configurations
            # that do not expose it.
            hypotheses = []

        return {
            "episode_id": episode.get("episode_id"),
            "configuration": configuration,
            "decisions": decisions,
            "terminal_status": terminal["status"],
            "terminal_hypothesis": terminal["hypothesis"],
            "evidence_refs": sorted(set(evidence_refs)),
            "hypotheses": hypotheses,
            "transitions": transitions,
            "challenges": challenges,
            "evidence_metadata": evidence_metadata,
            "execution_receipts": list(episode.get("runtime_receipts") or []),
            "adapter_failure": False,
            "adapter_failure_detail": [],
            "adapter_receipt": {
                "adapter": configuration,
                "runtime_events_observed": observed_events,
                "synthetic_semantic_events": 0,
                "notes": [],
            },
        }
    except (AdapterError, KeyError, TypeError, ValueError) as exc:
        return _failure_receipt(episode, [str(exc)], observed_events)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True)
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

    raw = load(args.raw)
    if raw.get("protocol") != "epistemic-process-v1":
        raise SystemExit("unexpected protocol")
    if raw.get("mode") not in (None, args.mode):
        raise SystemExit("adapter mode does not match raw runtime mode")
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

    receipts = [adapt_episode(ep) for ep in raw.get("episodes", [])]
    dump(args.out, receipts)

    summary = {
        "protocol": "epistemic-process-v1",
        "mode": args.mode,
        "receipts": len(receipts),
        "adapter_failures": sum(bool(r.get("adapter_failure")) for r in receipts),
        "synthetic_semantic_events": sum(
            int(r.get("adapter_receipt", {}).get("synthetic_semantic_events", 0))
            for r in receipts
        ),
    }
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
