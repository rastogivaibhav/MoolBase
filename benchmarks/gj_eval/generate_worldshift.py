#!/usr/bin/env python3
"""Deterministic WorldShift generator for GJ-Eval v1."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from dataclasses import dataclass
from typing import Any

ROOTS = ("deployment", "database", "network", "certificate")
VARIANTS = (
    "base",
    "duplicate_swarm",
    "correlated_sources",
    "false_majority",
    "late_contradiction",
    "source_invalidation",
    "supersession",
    "missing_evidence",
    "misleading_chronology",
    "minority_truth",
    "interaction_effect",
    "model_reversal",
)

TESTS = [
    {"id": "rollback", "label": "Roll back deployment canary", "cost": 3.0},
    {"id": "inspect_db", "label": "Inspect database saturation history", "cost": 1.0},
    {"id": "inspect_network", "label": "Inspect packet-loss history", "cost": 1.0},
    {"id": "inspect_cert", "label": "Inspect certificate validity/handshake logs", "cost": 1.0},
]


@dataclass(frozen=True)
class Evidence:
    evidence_id: str
    source_id: str
    source_family: str
    claim: str
    supports: str | None = None
    contradicts: str | None = None
    derived_from: str | None = None
    confidence: float = 0.8
    role: str = "supports"
    origin: str = "observed"
    observed_at: str = "2026-01-01T00:00:00Z"
    supersedes: str | None = None
    invalidates: str | None = None

    def as_dict(self) -> dict[str, Any]:
        out = {
            "evidence_id": self.evidence_id,
            "source_id": self.source_id,
            "source_family": self.source_family,
            "claim": self.claim,
            "confidence": self.confidence,
            "role": self.role,
            "origin": self.origin,
            "observed_at": self.observed_at,
        }
        for key in ("supports", "contradicts", "derived_from", "supersedes", "invalidates"):
            value = getattr(self, key)
            if value is not None:
                out[key] = value
        return out


def scenario_id(seed: int, group: int) -> str:
    digest = hashlib.sha256(f"{seed}:scenario:{group}".encode()).hexdigest()[:16]
    return f"scenario-{digest}"


def world_id(seed: int, group: int, variant: str) -> str:
    digest = hashlib.sha256(f"{seed}:{group}:{variant}".encode()).hexdigest()[:16]
    return f"ws-{digest}"


def observed_at(step: int) -> str:
    return f"2026-01-01T00:00:{step:02d}Z"


def support(root: str, step: int, n: int, family: str | None = None,
            derived: str | None = None) -> Evidence:
    family = family or f"{root}-family-{n}"
    return Evidence(
        evidence_id=f"e-{step}-{root}-{n}",
        source_id=f"{root}-source-{n}",
        source_family=family,
        claim=f"Observation supports {root} as causal.",
        supports=root,
        derived_from=derived,
        confidence=max(0.55, 0.92 - 0.04 * n),
        observed_at=observed_at(step),
    )


def contradiction(root: str, step: int, n: int = 0) -> Evidence:
    return Evidence(
        evidence_id=f"e-{step}-contra-{root}-{n}",
        source_id=f"contra-{root}-{n}",
        source_family=f"contra-{root}-family-{n}",
        claim=f"Observation contradicts {root} as the primary cause.",
        contradicts=root,
        confidence=0.93,
        role="contradicts",
        observed_at=observed_at(step),
    )


def build_world(seed: int, index: int, split: str) -> dict[str, Any]:
    group = index // len(VARIANTS)
    variant = VARIANTS[index % len(VARIANTS)]
    rng = random.Random(f"{seed}:scenario:{group}")
    truth = rng.choice(ROOTS)
    wrongs = [r for r in ROOTS if r != truth]
    decoy = rng.choice(wrongs)
    sid = scenario_id(seed, group)
    wid = world_id(seed, group, variant)
    timeline: list[dict[str, Any]] = []

    def add(step: int, *evidence: Evidence) -> None:
        timeline.append({"timestep": step, "observations": [e.as_dict() for e in evidence]})

    add(0, Evidence(
        evidence_id="e-0-symptom",
        source_id="service-monitor",
        source_family="service-monitor",
        claim="Checkout failures increased.",
        confidence=0.99,
        observed_at=observed_at(0),
    ))

    add(1, support(decoy, 1, 0, family="decoy-initial"))

    base_decoy = support(decoy, 2, 0, family="decoy-base-family")
    if variant == "duplicate_swarm":
        observations = [base_decoy]
        for j in range(1, 6):
            observations.append(Evidence(
                evidence_id=f"e-2-dup-{j}",
                source_id=f"republisher-{j}",
                source_family="decoy-base-family",
                claim=base_decoy.claim,
                supports=decoy,
                derived_from=base_decoy.evidence_id,
                confidence=base_decoy.confidence,
                observed_at=observed_at(2),
            ))
        add(2, *observations)
    elif variant == "correlated_sources":
        add(
            2,
            base_decoy,
            Evidence(
                evidence_id="e-2-correlated-1",
                source_id="correlated-republisher-1",
                source_family="decoy-base-family",
                claim=base_decoy.claim,
                supports=decoy,
                derived_from=base_decoy.evidence_id,
                confidence=base_decoy.confidence,
                observed_at=observed_at(2),
            ),
            Evidence(
                evidence_id="e-2-correlated-2",
                source_id="correlated-republisher-2",
                source_family="decoy-base-family",
                claim=base_decoy.claim,
                supports=decoy,
                derived_from=base_decoy.evidence_id,
                confidence=base_decoy.confidence,
                observed_at=observed_at(2),
            ),
        )
    elif variant == "false_majority":
        add(
            2,
            base_decoy,
            *[
                support(decoy, 2, j, family=f"false-majority-independent-{j}")
                for j in range(1, 4)
            ],
        )
    elif variant == "minority_truth":
        add(
            2,
            base_decoy,
            support(truth, 2, 9, family="truth-minority"),
        )
    else:
        add(2, base_decoy)

    if variant == "missing_evidence":
        add(3, contradiction(decoy, 3))
    elif variant == "misleading_chronology":
        add(3, Evidence(
            evidence_id="e-3-precedes",
            source_id="historical-metrics",
            source_family="historical-metrics",
            claim=f"{truth} degradation began before the {decoy} event.",
            supports=truth,
            contradicts=decoy,
            confidence=0.95,
            role="contradicts",
            observed_at=observed_at(3),
        ))
    else:
        add(3, contradiction(decoy, 3), support(truth, 3, 0, family="truth-independent-1"))

    if variant == "interaction_effect":
        partner = rng.choice([r for r in wrongs if r != decoy])
        add(
            4,
            support(truth, 4, 1, family="truth-independent-2"),
            Evidence(
                evidence_id="e-4-interaction",
                source_id="interaction-analysis",
                source_family="interaction-analysis",
                claim=f"{truth} and {partner} jointly amplify the symptom.",
                supports=truth,
                confidence=0.88,
                role="causal",
                observed_at=observed_at(4),
            ),
        )
    else:
        add(4, support(truth, 4, 1, family="truth-independent-2"))

    add(5, Evidence(
        evidence_id="e-5-intervention",
        source_id="operator-action",
        source_family="operator-action",
        claim=f"Intervention targeted {decoy}.",
        supports=decoy,
        confidence=0.5,
        observed_at=observed_at(5),
    ))

    add(6, Evidence(
        evidence_id="e-6-outcome",
        source_id="post-intervention-monitor",
        source_family="post-intervention-monitor",
        claim=f"Symptom persisted after intervening on {decoy}; evidence favors {truth}.",
        supports=truth,
        contradicts=decoy,
        confidence=0.98,
        role="contradicts",
        observed_at=observed_at(6),
    ))

    final = support(truth, 7, 2, family="truth-decisive")
    extras: list[Evidence] = [final]
    if variant == "source_invalidation":
        extras.append(Evidence(
            evidence_id="e-7-invalidate",
            source_id="audit",
            source_family="audit",
            claim="The initial decoy source was invalidated.",
            contradicts=decoy,
            invalidates=f"e-1-{decoy}-0",
            confidence=1.0,
            role="contradicts",
            observed_at=observed_at(7),
        ))
    elif variant == "supersession":
        extras.append(Evidence(
            evidence_id="e-7-supersede",
            source_id="authoritative-update",
            source_family="authoritative-update",
            claim=f"New authoritative evidence supersedes the early {decoy} report.",
            supports=truth,
            supersedes=f"e-1-{decoy}-0",
            confidence=1.0,
            role="supersedes",
            observed_at=7,
        ))
    elif variant == "late_contradiction":
        extras.append(contradiction(decoy, 7, 1))
    elif variant == "model_reversal":
        extras.append(Evidence(
            evidence_id="e-7-reversal",
            source_id="controlled-intervention",
            source_family="controlled-intervention",
            claim=f"Controlled intervention falsified {decoy} and isolated {truth}.",
            supports=truth,
            contradicts=decoy,
            confidence=1.0,
            role="causal",
            observed_at=7,
        ))
    add(7, *extras)

    optimal_test = {
        "deployment": "rollback",
        "database": "inspect_db",
        "network": "inspect_network",
        "certificate": "inspect_cert",
    }[truth]

    return {
        "schema_version": 1,
        "world_id": wid,
        "scenario_id": sid,
        "split": split,
        "variant": variant,
        "domain": "aiops",
        "choices": [{"id": r, "label": r.replace("_", " ").title()} for r in ROOTS] + [
            {"id": "unknown", "label": "Unknown / insufficient evidence"}
        ],
        "tests": TESTS,
        "timeline": timeline,
        "oracle": {
            "true_root": truth,
            "decoy_root": decoy,
            "decisive_timestep": 3,
            "insufficient_until": 2,
            "optimal_test": optimal_test,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--split", choices=("development", "validation", "test"), required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.count <= 0:
        raise SystemExit("--count must be positive")

    with open(args.output, "w", encoding="utf-8") as handle:
        for index in range(args.count):
            handle.write(json.dumps(build_world(args.seed, index, args.split), sort_keys=True))
            handle.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
