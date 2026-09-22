#!/usr/bin/env python3
"""G0 deterministic evidence baseline for GJ-Eval v1."""

from __future__ import annotations

import json
import sys
from collections import defaultdict
from typing import Any


THRESHOLD = 0.75


def main() -> int:
    task: dict[str, Any] = json.load(sys.stdin)
    visible = task.get("visible_state", [])
    if visible and not isinstance(visible[0], dict):
        raise ValueError("G0 requires structured canonical evidence")

    choices = [
        str(choice["id"])
        for choice in task["choices"]
        if choice["id"] != "unknown"
    ]
    per_family: dict[tuple[str, str, str], float] = {}
    for obs in visible:
        family = str(
            obs.get("source_family")
            or obs.get("source_id")
            or obs.get("evidence_id")
            or "unknown"
        )
        confidence = float(obs.get("confidence", 0.0))
        if obs.get("supports") in choices:
            key = (family, "supports", str(obs["supports"]))
            per_family[key] = max(per_family.get(key, 0.0), confidence)
        if obs.get("contradicts") in choices:
            key = (family, "contradicts", str(obs["contradicts"]))
            per_family[key] = max(per_family.get(key, 0.0), confidence)

    scores = defaultdict(float)
    for (_family, role, target), confidence in per_family.items():
        scores[target] += confidence if role == "supports" else -confidence

    positive = {choice: max(0.0, scores[choice]) for choice in choices}
    total = sum(positive.values())
    if total <= 0.0:
        root_choice = "unknown"
        probabilities = {choice: 0.0 for choice in choices}
        probabilities["unknown"] = 1.0
    else:
        probabilities = {choice: positive[choice] / total for choice in choices}
        probabilities["unknown"] = 0.0
        root_choice = max(choices, key=lambda c: (probabilities[c], c))

    selected = probabilities[root_choice]
    ranked = sorted(choices, key=lambda c: (-probabilities[c], c))
    print(json.dumps({
        "root_choice": root_choice,
        "act": "act" if root_choice != "unknown" and selected >= THRESHOLD else "review",
        "selected_confidence": selected,
        "choice_probabilities": probabilities,
        "ranked_hypotheses": ranked,
        "epistemic_status": "supported" if selected >= THRESHOLD else "provisional",
        "receipt": {
            "baseline": "g0_deterministic_evidence_family_dedup",
            "action_threshold": THRESHOLD,
            "family_contributions": len(per_family),
        },
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
