#!/usr/bin/env python3
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path

FAMILIES = {
    "duplicate_correlated_support",
    "late_refutation",
    "revocation",
    "insufficient_replacement",
    "genuine_ambiguity_tie",
    "evidence_accumulation",
    "correlated_majority_independent_minority",
    "stale_belief_recovery",
    "single_step_control",
}
RUNTIME_FIELDS = {"step", "id", "family", "kind", "bears_on", "depends_on", "revokes"}
EVALUATOR_ONLY = {
    "independent", "decisive", "challenge_warranted_steps", "oracle",
    "minimum_independent_families_for_resolution",
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("tasks")
    args = parser.parse_args()
    data = json.loads(Path(args.tasks).read_text(encoding="utf-8"))

    assert data["schema"] == "epistemic-process-v2-task-universe-v1"
    assert data["score_bearing_allowed"] is False
    assert data["episode_count"] == 270
    assert data["family_count"] == 9
    assert data["episodes_per_family"] == 30
    assert set(data["families"]) == FAMILIES
    assert set(data["runtime_fields"]) == RUNTIME_FIELDS
    assert not (set(data["runtime_fields"]) & EVALUATOR_ONLY)

    episodes = data["episodes"]
    ids = [ep["id"] for ep in episodes]
    assert len(ids) == len(set(ids)) == 270

    counts = collections.Counter(ep["task_family"] for ep in episodes)
    assert counts == collections.Counter({name: 30 for name in FAMILIES})

    all_event_ids: set[str] = set()
    mirror_counts = collections.Counter()
    for ep in episodes:
        assert 1 <= int(ep["variant"]) <= 30
        assert ep["mirror_primary"] in {"H1", "H2"}
        mirror_counts[(ep["task_family"], ep["mirror_primary"])] += 1
        assert isinstance(ep["challenge_warranted_steps"], list)
        oracle = ep["oracle"]
        assert oracle["terminal_operative"] in {"H1", "H2", None}
        assert oracle["terminal_committed"] in {"H1", "H2", None}

        prior: set[str] = set()
        steps = []
        for event in ep["events"]:
            event_id = str(event["id"])
            assert event_id not in all_event_ids
            all_event_ids.add(event_id)
            steps.append(int(event["step"]))
            assert event["bears_on"] in {"H1", "H2"}
            assert event["kind"] in {"support", "refute", "revoke"}
            for dep in event.get("depends_on") or []:
                assert dep in prior
            for revoked in event.get("revokes") or []:
                assert revoked in prior
            if event["kind"] == "revoke":
                assert event.get("revokes")
            prior.add(event_id)
        assert steps == list(range(1, len(steps) + 1))

    for family in FAMILIES:
        assert mirror_counts[(family, "H1")] == 15
        assert mirror_counts[(family, "H2")] == 15

    print(json.dumps({
        "task_universe": "passed",
        "episodes": 270,
        "families": 9,
        "per_family": 30,
        "mirror_balance": "15/15",
        "runtime_oracle_overlap": 0,
        "score_bearing_allowed": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
