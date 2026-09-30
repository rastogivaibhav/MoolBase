#!/usr/bin/env python3
"""Deterministically generate the EP-PROCESS-V2 240-episode task universe.

No runtime configuration or model output is consulted. The generator is
structural and mirrored across H1/H2 variants.
"""
from __future__ import annotations

import json
from pathlib import Path

FAMILIES = [
    "duplicate_correlated_support",
    "late_refutation",
    "revocation",
    "insufficient_replacement",
    "genuine_ambiguity_tie",
    "evidence_accumulation",
    "correlated_majority_independent_minority",
    "stale_belief_recovery",
    "single_step_control",
]
GENERATOR_SEED = 20261005


def event(step, event_id, family, kind, bears_on, **extra):
    row = {
        "step": step,
        "id": event_id,
        "family": family,
        "kind": kind,
        "bears_on": bears_on,
        "independent": True,
        "decisive": False,
    }
    row.update(extra)
    return row


def episode(family_name: str, variant: int):
    flip = variant % 2 == 0
    primary = "H2" if flip else "H1"
    alternative = "H1" if flip else "H2"
    prefix = f"{family_name.upper().replace('-', '_')}_{variant:02d}"
    fam = lambda suffix: f"{prefix}_{suffix}"
    evid = lambda n: f"{prefix}_e{n}"

    if family_name == "duplicate_correlated_support":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("A"), "support", primary,
                  independent=False, depends_on=[evid(1)]),
            event(3, evid(3), fam("B"), "support", alternative),
            event(4, evid(4), fam("C"), "support", primary),
        ]
        oracle = {"terminal_operative": primary,
                  "terminal_committed": primary,
                  "response_window": None}
        warranted = [3, 4]
    elif family_name == "late_refutation":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("B"), "support", primary),
            event(3, evid(3), fam("C"), "support", alternative),
            event(4, evid(4), fam("D"), "refute", primary, decisive=True),
            event(5, evid(5), fam("E"), "support", alternative),
        ]
        oracle = {
            "terminal_operative": alternative,
            "terminal_committed": alternative,
            "response_window": {
                "trigger_step": 4,
                "contradicted": primary,
                "acceptable_to": [alternative, None],
                "max_latency_steps": 1,
            },
        }
        warranted = [3, 4, 5]
    elif family_name == "revocation":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("B"), "support", primary),
            event(3, evid(3), fam("C"), "support", alternative),
            event(4, evid(4), fam("R"), "revoke", primary,
                  decisive=True, revokes=[evid(1), evid(2)]),
            event(5, evid(5), fam("D"), "support", alternative),
        ]
        oracle = {
            "terminal_operative": alternative,
            "terminal_committed": alternative,
            "response_window": {
                "trigger_step": 4,
                "contradicted": primary,
                "acceptable_to": [alternative, None],
                "max_latency_steps": 1,
            },
        }
        warranted = [4, 5]
    elif family_name == "insufficient_replacement":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("B"), "support", primary),
            event(3, evid(3), fam("R"), "refute", primary, decisive=True),
            event(4, evid(4), fam("C"), "support", alternative),
        ]
        oracle = {
            "terminal_operative": alternative,
            "terminal_committed": None,
            "response_window": {
                "trigger_step": 3,
                "contradicted": primary,
                "acceptable_to": [alternative, None],
                "max_latency_steps": 1,
            },
        }
        warranted = [3, 4]
    elif family_name == "genuine_ambiguity_tie":
        events = [
            event(1, evid(1), fam("A"), "support", "H1"),
            event(2, evid(2), fam("B"), "support", "H2"),
            event(3, evid(3), fam("A"), "support", "H1",
                  independent=False, depends_on=[evid(1)]),
            event(4, evid(4), fam("B"), "support", "H2",
                  independent=False, depends_on=[evid(2)]),
        ]
        oracle = {"terminal_operative": None,
                  "terminal_committed": None,
                  "response_window": None}
        warranted = [2, 3, 4]
    elif family_name == "evidence_accumulation":
        events = [
            event(1, evid(1), fam("A"), "support", alternative),
            event(2, evid(2), fam("B"), "support", primary),
            event(3, evid(3), fam("C"), "support", primary),
            event(4, evid(4), fam("D"), "support", primary),
        ]
        oracle = {"terminal_operative": primary,
                  "terminal_committed": primary,
                  "response_window": None}
        warranted = [2]
    elif family_name == "correlated_majority_independent_minority":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("A"), "support", primary,
                  independent=False, depends_on=[evid(1)]),
            event(3, evid(3), fam("A"), "support", primary,
                  independent=False, depends_on=[evid(1)]),
            event(4, evid(4), fam("B"), "support", alternative),
            event(5, evid(5), fam("C"), "support", alternative),
        ]
        oracle = {"terminal_operative": alternative,
                  "terminal_committed": alternative,
                  "response_window": None}
        warranted = [4, 5]
    elif family_name == "stale_belief_recovery":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
            event(2, evid(2), fam("B"), "support", primary),
            event(3, evid(3), fam("C"), "support", alternative),
            event(4, evid(4), fam("D"), "refute", primary, decisive=True),
            event(5, evid(5), fam("R"), "revoke", primary,
                  decisive=True, revokes=[evid(1)]),
            event(6, evid(6), fam("E"), "support", alternative),
        ]
        oracle = {
            "terminal_operative": alternative,
            "terminal_committed": alternative,
            "response_window": {
                "trigger_step": 4,
                "contradicted": primary,
                "acceptable_to": [alternative, None],
                "max_latency_steps": 2,
            },
        }
        warranted = [3, 4, 5, 6]
    elif family_name == "single_step_control":
        events = [
            event(1, evid(1), fam("A"), "support", primary),
        ]
        oracle = {
            "terminal_operative": primary,
            "terminal_committed": primary,
            "response_window": None,
        }
        warranted = []
    else:
        raise ValueError(family_name)

    return {
        "id": f"V2_{prefix}",
        "task_family": family_name,
        "variant": variant,
        "mirror_primary": primary,
        "minimum_independent_families_for_resolution": (
            1 if family_name == "single_step_control" else 2
        ),
        "challenge_warranted_steps": warranted,
        "oracle": oracle,
        "events": events,
    }


def generate():
    episodes = [
        episode(family_name, variant)
        for family_name in FAMILIES
        for variant in range(1, 31)
    ]
    return {
        "schema": "epistemic-process-v2-task-universe-v1",
        "status": "UNSCORED_PREREGISTERED_CANDIDATE",
        "experiment_id_candidate": "EP-PROCESS-V2-SCORE-001",
        "generator_seed": GENERATOR_SEED,
        "score_bearing_allowed": False,
        "family_count": 9,
        "episodes_per_family": 30,
        "episode_count": len(episodes),
        "runtime_fields": [
            "step", "id", "family", "kind", "bears_on",
            "depends_on", "revokes"
        ],
        "evaluator_only_fields": [
            "independent", "decisive", "challenge_warranted_steps",
            "oracle", "minimum_independent_families_for_resolution"
        ],
        "families": FAMILIES,
        "episodes": episodes,
    }


if __name__ == "__main__":
    target = Path(__file__).with_name("tasks_v2_candidate.json")
    target.write_text(
        json.dumps(generate(), indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"generated={target} episodes=270")
