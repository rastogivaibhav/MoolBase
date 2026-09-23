#!/usr/bin/env python3
"""Epistemic Process Evaluation v1 scorer.

This scorer is intentionally receipt-driven. It does not call an LLM and does not
infer missing events. Missing/malformed receipts remain failures in the report.
Before protocol freeze, all executions are UNSCORED dry runs.
"""
import argparse, json
from pathlib import Path

REQUIRED_RECEIPT_FIELDS = {
    "episode_id", "configuration", "decisions", "terminal_status",
    "terminal_hypothesis", "evidence_refs", "hypotheses", "transitions"
}

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def score_episode(task, receipt):
    missing = sorted(REQUIRED_RECEIPT_FIELDS - set(receipt))
    if missing:
        return {"episode_id": task["id"], "malformed": True, "missing": missing}
    events = {e["id"]: e for e in task["events"]}
    decisive = [e for e in task["events"] if e.get("decisive")]
    decisions = receipt.get("decisions", [])
    bearing = 0
    for d in decisions:
        refs = d.get("evidence_refs", [])
        h = d.get("hypothesis")
        if refs and all(r in events for r in refs) and any(events[r].get("bears_on") == h for r in refs):
            bearing += 1
    evidence_use = bearing / len(decisions) if decisions else 0.0
    refutation_response = False
    inertia = None
    if decisive:
        ref = decisive[0]
        contradicted = ref["bears_on"]
        response_steps = []
        for t in receipt.get("transitions", []):
            if t.get("step", -1) >= ref["step"] and t.get("type") in {"downgrade","reopen","revise"} and t.get("from") == contradicted:
                response_steps.append(t["step"])
        refutation_response = bool(response_steps)
        if response_steps:
            inertia = max(0, min(response_steps) - ref["step"])
        else:
            inertia = sum(1 for e in task["events"] if e["step"] > ref["step"])
    false_convergence = False
    for d in decisions:
        if d.get("status") == "resolved":
            h = d.get("hypothesis")
            step = d.get("step", 0)
            families = {e["family"] for e in task["events"] if e["step"] <= step and e.get("bears_on") == h and e.get("kind") == "support" and e.get("independent")}
            if len(families) < task["minimum_independent_families_for_resolution"]:
                false_convergence = True
    terminal_expected = task["terminal_supported"]
    terminal_correct = (receipt.get("terminal_status") == "abstain") if terminal_expected == "ABSTAIN" else (receipt.get("terminal_hypothesis") == terminal_expected)
    coverage = receipt.get("terminal_status") != "abstain"
    complete = all(receipt.get(k) not in (None, [], {}) for k in ["evidence_refs","hypotheses"]) and "transitions" in receipt
    return {
        "episode_id": task["id"], "malformed": False,
        "evidence_use_rate": evidence_use,
        "refutation_response": refutation_response,
        "revision_inertia_steps": inertia,
        "false_convergence": false_convergence,
        "coverage": coverage, "terminal_correct": terminal_correct,
        "receipt_complete": complete
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", required=True)
    ap.add_argument("--receipts", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mode", choices=["unscored-dry-run","score"], default="unscored-dry-run")
    args = ap.parse_args()
    tasks = load(args.tasks)
    if args.mode == "score" and not tasks.get("reporting", {}).get("score_bearing_allowed", False):
        raise SystemExit("score-bearing execution blocked: task manifest is not frozen")
    receipts = load(args.receipts)
    by_id = {r.get("episode_id"): r for r in receipts}
    rows = []
    for task in tasks["episodes"]:
        r = by_id.get(task["id"])
        rows.append({"episode_id":task["id"],"missing_receipt":True} if r is None else score_episode(task,r))
    result = {"protocol":tasks["protocol"],"mode":args.mode,"score_bearing":args.mode=="score","episodes":rows}
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(result, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))

if __name__ == "__main__":
    main()
