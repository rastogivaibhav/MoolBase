#!/usr/bin/env python3
"""Authorized evaluation wrapper for EP-PROCESS-V2-SCORE-001."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from claim_gate_v2 import evaluate_claim_gates
from evaluate_v2_candidate import evaluate
from statistics_v2 import BOOTSTRAP_RESAMPLES, BOOTSTRAP_SEED


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--raw", required=True)
    parser.add_argument("--measurement-model", required=True)
    parser.add_argument("--evaluation-out", required=True)
    parser.add_argument("--claims-out", required=True)
    args = parser.parse_args()

    tasks = json.loads(Path(args.tasks).read_text(encoding="utf-8"))
    raw = json.loads(Path(args.raw).read_text(encoding="utf-8"))
    model = json.loads(
        Path(args.measurement_model).read_text(encoding="utf-8")
    )
    if raw.get("experiment_id") != "EP-PROCESS-V2-SCORE-001":
        raise SystemExit("wrong score experiment id")
    if raw.get("score_bearing") is not True:
        raise SystemExit("raw evidence is not score-bearing")

    result = evaluate(tasks, raw, allow_score_bearing=True)
    evaluation = {
        "schema": "epistemic-process-v2-score-evaluation-v1",
        "experiment_id": "EP-PROCESS-V2-SCORE-001",
        "score_bearing": True,
        "bootstrap_seed": BOOTSTRAP_SEED,
        "bootstrap_resamples": BOOTSTRAP_RESAMPLES,
        **result,
    }
    claims = {
        "schema": "epistemic-process-v2-score-claims-v1",
        "experiment_id": "EP-PROCESS-V2-SCORE-001",
        "score_bearing": True,
        **evaluate_claim_gates(evaluation, model),
    }
    Path(args.evaluation_out).write_text(
        json.dumps(evaluation, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    Path(args.claims_out).write_text(
        json.dumps(claims, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps({
        "score_evaluation": "complete",
        "claim_gate_statuses": {
            key: value.get("status")
            for key, value in claims.items()
            if isinstance(value, dict) and "status" in value
        },
    }, sort_keys=True))


if __name__ == "__main__":
    main()
