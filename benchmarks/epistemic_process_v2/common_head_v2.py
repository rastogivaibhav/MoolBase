#!/usr/bin/env python3
"""Frozen V2 common decision head.

This is benchmark/control code, not production MoolBase logic. C0 and C1 must
use this exact function so persistent Graphene state is the only causal
difference between those profiles.
"""
from __future__ import annotations

from collections import defaultdict
from typing import Any, Iterable, Mapping

SUPPORT = "support"
OPPOSITION_KINDS = {"refute", "contradict", "revoke"}


def common_head(
    evidence_refs: Iterable[str],
    evidence_metadata: Mapping[str, Mapping[str, Any]],
) -> dict[str, Any]:
    support_families: dict[str, set[str]] = defaultdict(set)
    opposition_families: dict[str, set[str]] = defaultdict(set)
    used_refs: set[str] = set()

    for raw_ref in evidence_refs:
        ref = str(raw_ref)
        meta = evidence_metadata.get(ref)
        if not isinstance(meta, Mapping):
            continue
        target = str(meta.get("bears_on") or "")
        family = str(meta.get("family") or "")
        kind = str(meta.get("kind") or "").lower()
        if target not in {"H1", "H2"} or not family:
            continue
        if kind == SUPPORT:
            support_families[target].add(family)
            used_refs.add(ref)
        elif kind in OPPOSITION_KINDS:
            opposition_families[target].add(family)
            used_refs.add(ref)

    scores: dict[str, int] = {}
    for target in ("H1", "H2"):
        scores[target] = (
            len(support_families[target]) - len(opposition_families[target])
        )

    best = max(scores.values()) if scores else 0
    leaders = [target for target, score in scores.items() if score == best]
    operative = leaders[0] if best > 0 and len(leaders) == 1 else None

    return {
        "operative_hypothesis": operative,
        "committed_answer": None,
        "status": "operative_selected" if operative is not None else "open",
        "net_family_scores": scores,
        "support_families": {
            target: sorted(support_families[target]) for target in ("H1", "H2")
        },
        "opposition_families": {
            target: sorted(opposition_families[target]) for target in ("H1", "H2")
        },
        "evidence_refs": sorted(used_refs),
        "decision_head": "epistemic-process-v2-common-head-v1",
    }
