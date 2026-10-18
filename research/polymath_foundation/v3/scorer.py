#!/usr/bin/env python3
"""F3 scoring instrument, oracle and honest nonlearned sanity baselines.

No LLM is executed here. Reference-oracle accuracy is not learned competence.
"""
import argparse
import json
import math
from pathlib import Path
from challenge import EXPERIMENT, CARD_BY_DOMAIN, cards_from, sha, validate_packet


class AttemptError(ValueError):
    pass


def _int(x):
    return type(x) is int and x >= 0


def _finite(x):
    return type(x) in (float, int) and math.isfinite(x)


def oracle(task):
    """In-process deterministic scoring oracle, never included in tasks."""
    domain, x = task["domain"], task["inputs"]
    if domain == "networks":
        fields = ("queue", "arrivals", "service", "buffer")
        if any(k not in x or not _int(x[k]) for k in fields) or x["queue"] > x["buffer"]:
            return None
        unserved = max(0, x["queue"] + x["arrivals"] - x["service"])
        outputs = {"next_queue": min(x["buffer"], unserved),
                   "drops": max(0, unserved - x["buffer"])}
    elif domain == "biology":
        fields = ("observed", "setpoint", "gain", "limit", "disturbance")
        if (any(k not in x or not _finite(x[k]) for k in fields) or
                x["gain"] < 0 or x["limit"] <= 0):
            return None
        control = max(-x["limit"], min(x["limit"],
                      x["gain"]*(x["setpoint"]-x["observed"])))
        outputs = {"control": control,
                   "next_state": x["observed"] + x["disturbance"] + control}
    elif domain == "supply_chain":
        fields = ("daily_demand", "lead_days", "safety",
                  "on_hand", "on_order", "backorders", "target")
        if any(k not in x or not _int(x[k]) for k in fields):
            return None
        position = x["on_hand"] + x["on_order"] - x["backorders"]
        point = x["daily_demand"] * x["lead_days"] + x["safety"]
        qty = max(0, x["target"] - position) if position <= point else 0
        outputs = {"reorder_point": point, "order_qty": qty}
    else:
        raise AttemptError("unsupported domain")
    if task["target"] not in outputs:
        raise AttemptError("unsupported target")
    return outputs[task["target"]]


def response_packet(challenge, mode):
    """Instrument sanity baselines only, explicitly NOT learned agents."""
    if mode not in ("reference_oracle_not_learned",
                    "abstain_only_not_learned"):
        raise AttemptError("unknown selftest baseline")
    out = []
    for task in challenge["tasks"]:
        value = oracle(task)
        base = {"task_id": task["task_id"],
                "evidence_card_id": task["card_id"],
                "assumptions_acknowledged": True,
                "confidence": 1.0 if mode == "reference_oracle_not_learned" else 0.0}
        if mode == "reference_oracle_not_learned" and value is not None:
            out.append({**base, "answer_type": "numeric", "value": value})
        else:
            out.append({**base, "answer_type": "abstain",
                        "reason": "The declared mechanism cannot be applied with sufficient justified evidence."})
    return {"experiment": EXPERIMENT, "participant": mode,
            "challenge_sha256": sha(challenge), "responses": out,
            "model_executed": False}


def grade(challenge, cards, submission):
    validate_packet(challenge, cards)
    if submission.get("experiment") != EXPERIMENT:
        raise AttemptError("experiment mismatch")
    if submission.get("challenge_sha256") != sha(challenge):
        raise AttemptError("challenge mismatch / stale or forged attempt")
    if not isinstance(submission.get("participant"), str) or not submission["participant"].strip():
        raise AttemptError("no participant identity")
    if type(submission.get("model_executed")) is not bool:
        raise AttemptError("must declare whether a model was actually executed")
    if submission["participant"] in ("reference_oracle_not_learned",
                                     "abstain_only_not_learned") and submission["model_executed"]:
        raise AttemptError("cannot rebrand a scripted baseline as a learned model")
    rows = submission.get("responses")
    if not isinstance(rows, list) or len(rows) != len(challenge["tasks"]):
        raise AttemptError("missing or excess responses")
    attempts = {}
    for row in rows:
        if not isinstance(row, dict):
            raise AttemptError("non-record response")
        task_id = row.get("task_id")
        if not isinstance(task_id, str) or task_id in attempts:
            raise AttemptError("duplicate/empty task identity")
        if row.get("answer_type") not in ("abstain", "numeric"):
            raise AttemptError("unknown answer type")
        if row["answer_type"] == "numeric" and not _finite(row.get("value")):
            raise AttemptError("numeric response must be finite")
        if row["answer_type"] == "abstain" and (
                not isinstance(row.get("reason"), str) or len(row["reason"].strip()) < 12):
            raise AttemptError("abstention requires meaningful rationale")
        confidence = row.get("confidence")
        if confidence is not None and (not _finite(confidence) or
                                       not 0 <= confidence <= 1):
            raise AttemptError("invalid confidence")
        if type(row.get("assumptions_acknowledged")) is not bool:
            raise AttemptError("explicit assumption acknowledgment required")
        if not isinstance(row.get("evidence_card_id"), str):
            raise AttemptError("explicit evidence card required")
        attempts[task_id] = row
    if set(attempts) != {t["task_id"] for t in challenge["tasks"]}:
        raise AttemptError("unexpected or missing task IDs")
    results = []
    by_domain = {d: [] for d in CARD_BY_DOMAIN}
    for task in challenge["tasks"]:
        expected = oracle(task)
        r = attempts[task["task_id"]]
        boundary = expected is None
        cite_ok = r["evidence_card_id"] == task["card_id"]
        numeric = r["answer_type"] == "numeric"
        numerically_right = numeric and not boundary and abs(r["value"] - expected) <= 1e-9
        correct = bool(cite_ok and r["assumptions_acknowledged"] and
                       ((boundary and not numeric) or numerically_right))
        receipt = {
            "task_id": task["task_id"], "domain": task["domain"],
            "is_boundary_case": boundary, "correct": correct,
            "citation_valid": cite_ok,
            "wrong_numeric_or_unsupported": numeric and (
                boundary or not numerically_right or not cite_ok),
            "answered_numeric": numeric, "abstained": not numeric
        }
        results.append(receipt)
        by_domain[task["domain"]].append(receipt)

    def metrics(records):
        n = len(records)
        valid = [r for r in records if not r["is_boundary_case"]]
        boundary = [r for r in records if r["is_boundary_case"]]
        return {
            "n": n, "valid_application_n": len(valid), "boundary_n": len(boundary),
            "correct_total": sum(r["correct"] for r in records),
            "total_accuracy": sum(r["correct"] for r in records) / n if n else None,
            "application_accuracy": sum(r["correct"] for r in valid)/len(valid) if valid else None,
            "boundary_abstention_rate": sum(r["correct"] for r in boundary)/len(boundary) if boundary else None,
            "numeric_coverage": sum(r["answered_numeric"] for r in valid)/len(valid) if valid else None,
            "unsupported_numeric_rate": sum(r["wrong_numeric_or_unsupported"] for r in records)/n if n else None,
            "citation_valid_rate": sum(r["citation_valid"] for r in records)/n if n else None
        }

    return {
        "experiment": EXPERIMENT,
        "participant": submission["participant"],
        "model_executed": submission["model_executed"],
        "claim_status": ("MODEL_RESULT_NEEDS_INDEPENDENT_REVIEW" if submission["model_executed"]
                         else "EVALUATOR_SELFTEST_NOT_MODEL_LEARNING"),
        "challenge_sha256": sha(challenge),
        "metrics": metrics(results),
        "per_domain": {d: metrics(rows) for d, rows in by_domain.items()},
        "case_receipts_without_gold": results,
        "limitations": [
            "Open generator and oracle; not independently blinded",
            "Synthetic mechanism cards are pedagogical, not verified real-world science",
            "Scripted reference solver success does not demonstrate acquired comprehension",
            "No authentic external source contents or pretrained LLM were evaluated"
        ]
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cards", required=True)
    p.add_argument("--challenge", required=True)
    p.add_argument("--out", required=True)
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest", choices=["reference_oracle_not_learned",
                                             "abstain_only_not_learned"])
    group.add_argument("--submission")
    args = p.parse_args()
    cards = cards_from(args.cards)
    challenge = json.loads(Path(args.challenge).read_text(encoding="utf-8"))
    submission = (response_packet(challenge, args.selftest) if args.selftest
                  else json.loads(Path(args.submission).read_text(encoding="utf-8")))
    report = grade(challenge, cards, submission)
    output = Path(args.out)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"f3_scoring=PASS participant={report['participant']} total_accuracy={report['metrics']['total_accuracy']:.6f} external_comprehension=NOT_ASSESSED")


if __name__ == "__main__":
    main()
