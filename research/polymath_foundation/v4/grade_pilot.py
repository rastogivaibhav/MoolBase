#!/usr/bin/env python3
"""Score only executed frozen 24-task actual-model subset using unchanged v3 oracle."""
import argparse
import json
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "v3"))
from challenge import cards_from, generate, sha
from scorer import grade, AttemptError
from run_actual_model import choose_tasks


def metrics(rows):
    if not rows:
        return {"n": 0}
    valid = [r for r in rows if not r["is_boundary_case"]]
    boundary = [r for r in rows if r["is_boundary_case"]]
    return {
        "n": len(rows),
        "valid_n": len(valid),
        "boundary_n": len(boundary),
        "correct_n": sum(r["correct"] for r in rows),
        "total_accuracy": sum(r["correct"] for r in rows) / len(rows),
        "valid_application_accuracy": (sum(r["correct"] for r in valid) /
                                      len(valid)) if valid else None,
        "boundary_abstention_rate": (sum(r["correct"] for r in boundary) /
                                    len(boundary)) if boundary else None,
        "citation_valid_rate": sum(r["citation_valid"] for r in rows) / len(rows),
        "unsupported_numeric_rate": sum(r["wrong_numeric_or_unsupported"]
                                         for r in rows) / len(rows),
        "numeric_coverage_valid": (sum(r["answered_numeric"] for r in valid) /
                                   len(valid)) if valid else None
    }


def evaluate(cards, responses):
    master, tasks = choose_tasks(cards)
    selected = [x["task_id"] for x in tasks]
    if responses.get("status") != "ACTUAL_MODEL_RESPONSES":
        raise ValueError("source is not an actual-model inference receipt")
    if responses.get("experiment") != "MOOL-POLYMATH-F4-001":
        raise ValueError("wrong model pilot")
    if responses.get("full_challenge_sha256") != sha(master):
        raise ValueError("source model output was from a different challenge")
    if responses.get("sample_task_ids") != selected:
        raise ValueError("sample is not preregistered")
    if not isinstance(responses.get("model_sha256"), str) or len(responses["model_sha256"]) != 64:
        raise ValueError("actual model binary file hash missing")

    result = {
        "experiment": "MOOL-POLYMATH-F4-001",
        "status": "EXECUTED_MODEL_PILOT_NO_BLIND_SCIENTIFIC_CLAIM",
        "model": responses["model_name"],
        "model_sha256": responses["model_sha256"],
        "server_git_commit": responses["server_git_commit"],
        "git_commit": responses["runner_git_commit"],
        "full_challenge_sha256": sha(master),
        "sample_task_ids": selected,
        "full_v3_question_n": 72,
        "pilot_question_n": 24,
        "arms": {},
        "hypokosh_dwm": "NOT_RUN",
        "moolbase_native_retrieval": "NOT_RUN",
        "limitations": [
            "Open toy mathematical cards and generator; no independent blind questions",
            "No MoolBase native learner, ontology retrieval, HypoKosh or DWM integrated",
            "Prompt lengths/total compute budgets not matched across arms",
            "24 predetermined pilot cases too small for a robust generalisation claim",
            "Card knowledge is supplied at query time, not acquired autonomously"
        ]
    }
    by_arm = {}
    for arm in ("A0", "A1"):
        meta = responses.get("arms", {}).get(arm)
        if not meta or meta.get("model_executed") is not True:
            raise ValueError("arm missing actual model inference: " + arm)
        recs = meta.get("records")
        if len(recs) != 24 or [r["task_id"] for r in recs] != selected:
            raise ValueError("invalid/fabricated arm task sequence")
        rawcount = sum(not isinstance(r.get("raw_model_output"), str) for r in recs)
        if rawcount:
            raise ValueError("missing raw model completions")
        converted = {r["task_id"]: r["adapted_response"] for r in recs}
        full = []
        for task in master["tasks"]:
            tid = task["task_id"]
            if tid in converted:
                full.append(converted[tid])
            else:
                # Unsampled padding is unconditionally wrong thanks to
                # deliberately invalid citation and false acknowledgment.
                full.append({
                    "task_id": tid, "answer_type": "abstain",
                    "reason": "Outside the frozen 24-task pilot subset.",
                    "evidence_card_id": "NOT_SAMPLED_NO_CITATION",
                    "assumptions_acknowledged": False,
                    "confidence": 0.0
                })
        attempt = {
            "experiment": "MOOL-POLYMATH-F3-001",
            "participant": "Qwen2.5-0.5B-Instruct-" + arm,
            "challenge_sha256": sha(master),
            "model_executed": True,
            "responses": full
        }
        original = grade(master, cards, attempt)
        receipts = [r for r in original["case_receipts_without_gold"]
                    if r["task_id"] in converted]
        if len(receipts) != 24:
            raise ValueError("subset was not aligned with oracle receipts")
        # An invalid model output has been mapped to an invalid citation;
        # ensure it cannot earn credit even when abstention is correct.
        errors = [r for r in recs if r["parser_error"]]
        for bad in errors:
            scored = next(r for r in receipts if r["task_id"] == bad["task_id"])
            if scored["correct"]:
                raise ValueError("parser failure wrongly earned credit")
        by_arm[arm] = receipts
        result["arms"][arm] = {
            "score": metrics(receipts),
            "per_domain": {domain: metrics([r for r in receipts if r["domain"] == domain])
                           for domain in ("networks", "biology", "supply_chain")},
            "parser_failure_count": len(errors),
            "mean_elapsed_seconds": statistics.mean(r["elapsed_seconds"] for r in recs),
            "total_output_tokens_reported": sum(
                (r.get("usage") or {}).get("completion_tokens", 0) for r in recs),
            "case_receipts": receipts
        }
    by_id = {r["task_id"]: r for r in by_arm["A0"]}
    result["paired_difference_A1_minus_A0"] = (
        result["arms"]["A1"]["score"]["total_accuracy"] -
        result["arms"]["A0"]["score"]["total_accuracy"]
    )
    result["pair_wins_A1_only"] = sum(
        r["correct"] and not by_id[r["task_id"]]["correct"]
        for r in by_arm["A1"])
    result["pair_wins_A0_only"] = sum(
        not r["correct"] and by_id[r["task_id"]]["correct"]
        for r in by_arm["A1"])
    return result


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--cards",required=True)
    p.add_argument("--inference",required=True)
    p.add_argument("--out",required=True)
    args=p.parse_args()
    cards=cards_from(args.cards)
    receipts=json.loads(Path(args.inference).read_text(encoding="utf-8"))
    result=evaluate(cards,receipts)
    target=Path(args.out)
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",
                      encoding="utf-8")
    for arm in ("A0","A1"):
        s=result["arms"][arm]["score"]
        print(f"{arm}: correct={s['correct_n']}/{s['n']} "
              f"valid_accuracy={s['valid_application_accuracy']:.3f} "
              f"boundary_abstention={s['boundary_abstention_rate']} "
              f"parse_failures={result['arms'][arm]['parser_failure_count']}")
    print("f4_actual_model_grading=PASS "
          "MoolBase_native=NOT_RUN HypoKosh_DWM=NOT_RUN")


if __name__=="__main__":
    main()
