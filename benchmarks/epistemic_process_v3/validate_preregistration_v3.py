#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent

REQUIRED = [
    "PREREGISTRATION.md",
    "semantic_contract_v3.json",
    "measurement_model_v3.json",
    "metric_definitions_v3.json",
    "telemetry_contract_v3.json",
    "claim_gates_v3.json",
    "failure_policy_v3.json",
    "statistics_plan_v3.json",
    "task_family_requirements_v3.json",
    "controls_v3.json",
    "validate_preregistration_v3.py",
    "test_metric_semantics_v3.py",
    "test_claim_gates_v3.py",
]

JSON_FILES = [name for name in REQUIRED if name.endswith(".json")]


def load(name):
    return json.loads((ROOT / name).read_text(encoding="utf-8"))


def walk_no_none(value, path="root"):
    if value is None:
        raise AssertionError(f"null preregistration value at {path}")
    if isinstance(value, dict):
        for k, v in value.items():
            walk_no_none(v, f"{path}.{k}")
    elif isinstance(value, list):
        for i, v in enumerate(value):
            walk_no_none(v, f"{path}[{i}]")


def main():
    for name in REQUIRED:
        path = ROOT / name
        assert path.exists(), f"missing required preregistration file: {name}"

    docs = {name: load(name) for name in JSON_FILES}
    for name, doc in docs.items():
        assert doc.get("score_bearing_authorized") is False, name
        assert doc.get("status") == "FROZEN_PRE_IMPLEMENTATION", name

    sem = docs["semantic_contract_v3.json"]
    assert sem["tie"]["continuous_comparison"]["absolute_tolerance"] == 1e-9
    assert sem["tie"]["continuous_comparison"]["relative_tolerance"] == 1e-6
    assert sem["incumbent_replacement_policy"]["operative_hypothesis"] is None
    assert sem["incumbent_replacement_policy"]["epistemic_status"] == "contested"
    assert sem["dwm"]["visited_state_growth_alone_is_useful"] is False

    tasks = docs["task_family_requirements_v3.json"]
    assert tasks["planned_episode_count"] == 384
    assert tasks["family_count"] == 24
    assert tasks["episodes_per_family"] == 16
    assert len(tasks["families"]) == 24
    ids = [row["id"] for row in tasks["families"]]
    assert len(ids) == len(set(ids)) == 24
    assert tasks["final_tasks_created_in_cycle2"] is False

    controls = docs["controls_v3.json"]
    assert [p["id"] for p in controls["profiles"]] == [
        "C0", "G0E", "C1", "G1", "G2"
    ]
    assert controls["score_bearing_additional_ablations"] == []

    metrics = docs["metric_definitions_v3.json"]
    definitions = metrics["definitions"]
    for metric, spec in definitions.items():
        for field in (
            "numerator", "denominator", "eligibility",
            "missing", "runtime_failure", "direction", "role",
        ):
            assert field in spec, f"{metric} missing {field}"

    gates = docs["claim_gates_v3.json"]
    allowed_metrics = set(definitions) | set(gates["derived_metrics"])
    for claim, spec in gates["claims"].items():
        assert spec.get("comparison"), f"{claim} lacks comparison/control"
        assert spec.get("all"), f"{claim} lacks frozen thresholds"
        for metric, op, threshold in spec["all"]:
            assert metric in allowed_metrics, f"{claim}: undefined metric {metric}"
            assert op in {">=", "<=", ">", "<", "=="}
            assert threshold is not None

    model = docs["measurement_model_v3.json"]
    assert model["planned_episode_count"] == 384
    assert model["planned_family_count"] == 24
    assert len(model["hypotheses"]) >= 12
    walk_no_none(model["thresholds"], "measurement_model.thresholds")

    stats = docs["statistics_plan_v3.json"]
    assert stats["alpha"] == 0.05
    assert stats["bootstrap_seed"] == 20261002
    assert stats["bootstrap_resamples"] == 20000
    assert stats["multiple_testing"].startswith("Holm")
    assert stats["sample_size"]["post_hoc_power_forbidden"] is True

    failure = docs["failure_policy_v3.json"]
    assert failure["automatic_score_retry"] is False
    assert failure["score_run_count"] == 1
    assert failure["imputation"] is False
    assert failure["rerun_forbidden"] is True
    assert "oracle_leakage" in failure["global_invalidation"]
    assert "missing_task_config_pair" in failure["global_invalidation"]

    telemetry = docs["telemetry_contract_v3.json"]
    assert telemetry["behavior_influencing"] is False
    assert telemetry["canonical_family_id"] == "family:<raw-id>"
    assert "expansion_opportunity_class" in telemetry["dwm_before_fields"]

    forbidden = set(tasks["runtime_forbidden_fields"])
    required_forbidden = {
        "expected_terminal_answer",
        "correctness",
        "challenge_warranted",
        "revision_required",
        "future_observations",
        "expected_dwm_usefulness",
        "oracle",
    }
    assert required_forbidden <= forbidden

    # Cycle 2 must not create final V3 tasks or a score authorization manifest.
    assert not (ROOT / "tasks_v3.json").exists()
    assert not (ROOT / "score_freeze_v3.json").exists()

    print(
        "cycle2_preregistration_validation=passed "
        "files=13 families=24 planned_episodes=384 score_authorized=false"
    )


if __name__ == "__main__":
    main()
