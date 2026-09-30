#!/usr/bin/env python3
from __future__ import annotations

import copy

from evaluate_v2_candidate import (
    CONFIGS,
    EvaluationInputError,
    evaluate,
    validate_inputs,
)


def task(target="H1", expected_commit="H1"):
    return {
        "id": "SYNTH_01",
        "task_family": "synthetic",
        "minimum_independent_families_for_resolution": 2,
        "challenge_warranted_steps": [],
        "oracle": {
            "terminal_operative": target,
            "terminal_committed": expected_commit,
            "response_window": None,
        },
        "events": [
            {
                "step": 1,
                "id": "e1",
                "family": "F1",
                "kind": "support",
                "bears_on": target,
                "independent": True,
                "decisive": False,
            },
            {
                "step": 2,
                "id": "e2",
                "family": "F2",
                "kind": "support",
                "bears_on": target,
                "independent": True,
                "decisive": False,
            },
        ],
    }


def state(op, committed, status, families):
    return {
        "has_answer": op is not None,
        "operative_hypothesis": op,
        "committed_answer": committed,
        "epistemic_status": status,
        "confidence": 0.9 if op is not None else 0.0,
        "bundle_hash": 1,
        "visited_states": 1,
        "truncated": False,
        "evidence_edge_ids": [],
        "evidence_family_ids": list(families),
        "dependency_lineage_ids": [],
        "target_ranking": [],
    }


def null_state():
    return state(None, None, "none", [])


def episode(config, target="H1", expected_commit="H1"):
    metadata = {
        "e1": {
            "family": "F1",
            "kind": "support",
            "bears_on": target,
            "depends_on": [],
            "revokes": [],
        },
        "e2": {
            "family": "F2",
            "kind": "support",
            "bears_on": target,
            "depends_on": [],
            "revokes": [],
        },
    }
    if config == "G0E":
        finals = [
            state(None, None, "evidence_only", ["F1"]),
            state(None, None, "evidence_only", ["F1", "F2"]),
        ]
    elif config in {"C0", "C1"}:
        finals = [
            state(target, None, "operative_selected", ["F1"]),
            state(
                target,
                None,
                "operative_selected",
                ["F2"] if config == "C0" else ["F1", "F2"],
            ),
        ]
    else:
        finals = [
            state(target, target, "provisionally_resolved", ["F1"]),
            state(target, expected_commit, "resolved", ["F1", "F2"]),
        ]

    active = {
        "C0": [["e1"], ["e2"]],
        "G0E": [["e1"], ["e1", "e2"]],
        "C1": [["e1"], ["e1", "e2"]],
        "G1": [["e1"], ["e1", "e2"]],
        "G2": [["e1"], ["e1", "e2"]],
    }[config]
    steps = []
    previous = null_state()
    for i, final in enumerate(finals, start=1):
        steps.append({
            "step": i,
            "ingested_evidence_ids": [f"e{i}"],
            "previous_step_state": previous,
            "initial_state": copy.deepcopy(final),
            "recovery_rounds": [],
            "final_state": copy.deepcopy(final),
            "native_events": [],
            "active_evidence_refs": active[i - 1],
            "telemetry_gaps": [],
            "execution": {
                "execution_seconds": None,
                "expansion_rounds": 0,
                "visited_states_total": i,
                "evidence_edges_considered": i,
                "runtime_failure": False,
            },
        })
        previous = copy.deepcopy(final)
    return {
        "episode_id": "SYNTH_01",
        "configuration": config,
        "evidence_metadata": metadata,
        "steps": steps,
        "failures": [],
        "execution_seconds": 0.001,
    }


def fixture(target="H1", expected_commit="H1"):
    tasks = {"episodes": [task(target, expected_commit)]}
    raw = {
        "score_bearing": False,
        "score_bearing_authorized": False,
        "configurations": list(CONFIGS),
        "episodes": [
            episode(config, target, expected_commit)
            for config in CONFIGS
        ],
    }
    return tasks, raw


def expect_rejected(name, mutate):
    tasks, raw = fixture()
    mutate(tasks, raw)
    try:
        validate_inputs(tasks, raw)
    except EvaluationInputError:
        return
    raise AssertionError(f"adversarial case was accepted: {name}")


def main() -> None:
    tasks, raw = fixture()
    result = evaluate(tasks, raw)
    assert result["aggregate"]["C0"]["operative_hypothesis_accuracy"] == 1.0
    assert result["aggregate"]["C1"]["evidence_retention_exactness"] == 1.0
    assert result["aggregate"]["G0E"]["evidence_retention_exactness"] == 1.0
    assert result["aggregate"]["G1"]["committed_accuracy"] == 1.0
    assert result["aggregate"]["G2"]["earned_resolution_rate"] == 1.0
    assert result["comparisons"]["C1->G1"][
        "commitment_causal_comparison_authorized"
    ] is False

    mirrored_tasks, mirrored_raw = fixture("H2", "H2")
    mirrored = evaluate(mirrored_tasks, mirrored_raw)
    for config in CONFIGS:
        assert (
            mirrored["aggregate"][config]["operative_hypothesis_accuracy"]
            == result["aggregate"][config]["operative_hypothesis_accuracy"]
        )

    abstain_tasks, abstain_raw = fixture("H1", None)
    for ep in abstain_raw["episodes"]:
        if ep["configuration"] in {"G1", "G2"}:
            ep["steps"][-1]["final_state"]["committed_answer"] = None
            ep["steps"][-1]["final_state"]["epistemic_status"] = "contested"
            ep["steps"][-1]["initial_state"]["committed_answer"] = None
            ep["steps"][-1]["initial_state"]["epistemic_status"] = "contested"
    abstain = evaluate(abstain_tasks, abstain_raw)
    assert abstain["aggregate"]["G1"]["appropriate_abstention_rate"] == 1.0
    assert abstain["aggregate"]["G2"]["appropriate_abstention_rate"] == 1.0

    expect_rejected(
        "duplicate episode",
        lambda t, r: r["episodes"].append(copy.deepcopy(r["episodes"][0])),
    )
    expect_rejected(
        "missing episode",
        lambda t, r: r["episodes"].pop(),
    )
    expect_rejected(
        "configuration mixing",
        lambda t, r: r["episodes"][0].__setitem__("configuration", "G2"),
    )
    expect_rejected(
        "duplicate configuration declaration",
        lambda t, r: r["configurations"].append("C0"),
    )
    expect_rejected(
        "oracle leakage",
        lambda t, r: r["episodes"][0].__setitem__("oracle", {"x": 1}),
    )
    expect_rejected(
        "metadata manipulation",
        lambda t, r: r["episodes"][1]["evidence_metadata"]["e1"].__setitem__(
            "family", "FAKE"
        ),
    )
    expect_rejected(
        "cross-episode evidence leakage",
        lambda t, r: r["episodes"][2]["steps"][1][
            "active_evidence_refs"
        ].append("other_episode_evidence"),
    )
    expect_rejected(
        "missing step",
        lambda t, r: r["episodes"][3]["steps"].pop(),
    )
    expect_rejected(
        "wrong ingested evidence",
        lambda t, r: r["episodes"][3]["steps"][1].__setitem__(
            "ingested_evidence_ids", ["e1"]
        ),
    )
    expect_rejected(
        "commit under contested state",
        lambda t, r: (
            r["episodes"][3]["steps"][1]["final_state"].__setitem__(
                "epistemic_status", "contested"
            )
        ),
    )
    expect_rejected(
        "fabricated cross-step transition",
        lambda t, r: r["episodes"][2]["steps"][1]["native_events"].append(
            {"type": "cross_step_belief_change"}
        ),
    )
    expect_rejected(
        "fabricated revision",
        lambda t, r: r["episodes"][4]["steps"][1]["native_events"].append(
            {"type": "revision"}
        ),
    )

    def missing_cross_step(t, r):
        ep = next(x for x in r["episodes"] if x["configuration"] == "C1")
        ep["steps"][1]["initial_state"]["operative_hypothesis"] = "H2"
        ep["steps"][1]["final_state"]["operative_hypothesis"] = "H2"

    expect_rejected("missing cross-step transition", missing_cross_step)

    def missing_revision(t, r):
        ep = next(x for x in r["episodes"] if x["configuration"] == "G2")
        ep["steps"][1]["recovery_rounds"] = [{
            "trigger": "none",
            "operative_hypothesis_changed": True,
        }]

    expect_rejected("missing revision event", missing_revision)

    def fake_dwm_round(t, r):
        ep = next(x for x in r["episodes"] if x["configuration"] == "G2")
        ep["steps"][1]["recovery_rounds"] = [{
            "trigger": "dwm_opposition",
            "operative_hypothesis_changed": False,
        }]

    expect_rejected("DWM round without challenge/reopen", fake_dwm_round)

    print("cycle6_evaluator_adversarial_contracts=passed cases=16")


if __name__ == "__main__":
    main()
