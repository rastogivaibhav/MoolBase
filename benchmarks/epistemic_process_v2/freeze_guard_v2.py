#!/usr/bin/env python3
"""Pre-freeze and score-authorization guard for Epistemic Process V2."""
from __future__ import annotations
import argparse, json
from pathlib import Path
from run_v2 import RUNTIME_FIELDS
from score_gate_v2 import verify_score_gate

ROOT=Path(__file__).resolve().parent
PREREG=ROOT/"preregistration_v2.json"
TASKS=ROOT/"tasks_v2.json"
DEFAULT_MANIFEST=ROOT/"score_freeze_v2.json"

def build_report(manifest: Path) -> dict:
    prereg=json.loads(PREREG.read_text(encoding="utf-8"))
    tasks=json.loads(TASKS.read_text(encoding="utf-8"))
    hidden=set(tasks.get("oracle_fields_not_sent_to_sut") or [])
    leaked=sorted(hidden & set(RUNTIME_FIELDS))
    gate=verify_score_gate(manifest) if manifest.exists() else {
        "valid":False,"errors":["freeze manifest not present"],"experiment_id":None
    }
    return {
        "protocol":"epistemic-process-v2",
        "preregistered_unscored": prereg.get("status")=="PREREGISTERED_UNSCORED",
        "legacy_score_authorization": prereg.get("score_bearing_runs_allowed"),
        "runtime_input_fields":list(RUNTIME_FIELDS),
        "hidden_oracle_fields":sorted(hidden),
        "oracle_fields_leaked_to_runtime":leaked,
        "freeze_manifest_present":manifest.exists(),
        "score_gate_valid":bool(gate.get("valid")),
        "score_gate_errors":list(gate.get("errors") or []),
        "experiment_id":gate.get("experiment_id"),
        "freeze_ready":not leaked and prereg.get("score_bearing_runs_allowed") is False,
        "score_bearing_locked":not bool(gate.get("valid")),
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--freeze-manifest",default=str(DEFAULT_MANIFEST))
    ap.add_argument("--require-freeze-ready",action="store_true")
    ap.add_argument("--require-score-gate",action="store_true")
    args=ap.parse_args()
    report=build_report(Path(args.freeze_manifest))
    print(json.dumps(report,indent=2,sort_keys=True))
    if args.require_freeze_ready and not report["freeze_ready"]:
        raise SystemExit("freeze blocked: V2 runtime/oracle boundary not ready")
    if args.require_score_gate and not report["score_gate_valid"]:
        raise SystemExit("score gate blocked")

if __name__=="__main__":
    main()
