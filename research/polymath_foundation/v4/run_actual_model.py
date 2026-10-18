#!/usr/bin/env python3
"""Execute ACTUAL local LLM inference for F4 A0/A1; never fabricate outputs.

The preregistered candidate is Qwen2.5-0.5B-Instruct GGUF served through
llama.cpp's OpenAI-compatible /v1/chat/completions endpoint.
"""
import argparse
import hashlib
import json
import math
import os
import time
import urllib.request
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "v3"))
from challenge import cards_from, generate, sha, validate_packet, EXPERIMENT

V4 = "MOOL-POLYMATH-F4-001"
SYSTEM = (
    "Solve one applied mathematical task. Use ONLY the supplied information; "
    "if assumptions do not hold, abstain with a precise boundary explanation. "
    "Return a single JSON object, no Markdown or extra text. "
    'Keys: {"answer_type":"numeric" or "abstain",'
    '"value":number (numeric only), "reason":string (abstain only),'
    '"evidence_card_id":string, "assumptions_acknowledged":true,'
    '"confidence":number between 0 and 1}. '
    "Cite the task's card ID; do not guess a number when inputs are invalid."
)


def choose_tasks(cards):
    master = generate(cards)
    assert len(master["tasks"]) == 72
    counts = {}
    chosen = []
    for task in master["tasks"]:
        d = task["domain"]
        n = counts.get(d, 0)
        if n < 8:
            chosen.append(task)
            counts[d] = n + 1
    if len(chosen) != 24 or set(counts.values()) != {8}:
        raise ValueError("frozen 8-per-domain selection violated")
    return master, chosen


def build_prompt(task, card, arm):
    body = ("Domain: " + task["domain"] + "\n"
            "Card ID: " + task["card_id"] + "\n"
            "Requested result: " + task["target"] + "\n"
            "Inputs: " + json.dumps(task["inputs"], sort_keys=True) + "\n"
            "Question: " + task["prompt"])
    if arm == "A1":
        body = ("INSTRUCTIONAL CARD:\n" + card["title"] + "\n" +
                card["description"] + "\nAssumptions: " +
                "; ".join(card["assumptions"]) +
                "\nAbstain if: " + "; ".join(card["must_abstain_when"]) +
                "\n\n" + body)
    return body


def call_server(base_url, model_name, messages, timeout):
    request = urllib.request.Request(
        base_url.rstrip("/") + "/v1/chat/completions",
        data=json.dumps({
            "model": model_name, "messages": messages,
            "temperature": 0, "seed": 20261018,
            "max_tokens": 120, "stream": False,
            "response_format": {"type": "json_object"}
        }).encode(),
        headers={"Content-Type": "application/json"},
        method="POST")
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def reject(task, explanation):
    # An adapter failure MUST receive zero credit, even on a boundary task.
    return {
        "task_id": task["task_id"], "answer_type": "abstain",
        "reason": "Invalid generated model output; " + explanation,
        "evidence_card_id": "INVALID_MODEL_OUTPUT_NOT_EVIDENCE",
        "assumptions_acknowledged": False, "confidence": 0.0
    }


def adapt(raw, task):
    try:
        obj = json.loads(raw)
        if type(obj) is not dict:
            raise ValueError("expected JSON object")
        mode = obj.get("answer_type")
        if mode not in ("numeric", "abstain"):
            raise ValueError("missing valid answer_type")
        if type(obj.get("evidence_card_id")) is not str:
            raise ValueError("missing evidence_card_id")
        if type(obj.get("assumptions_acknowledged")) is not bool:
            raise ValueError("missing assumptions_acknowledged")
        c = obj.get("confidence")
        if c is not None and (type(c) not in (int, float) or
                              not math.isfinite(c) or not 0 <= c <= 1):
            raise ValueError("invalid confidence")
        converted = {
            "task_id": task["task_id"],
            "answer_type": mode,
            "evidence_card_id": obj["evidence_card_id"],
            "assumptions_acknowledged": obj["assumptions_acknowledged"]
        }
        if c is not None:
            converted["confidence"] = c
        if mode == "numeric":
            val = obj.get("value")
            if type(val) not in (int, float) or not math.isfinite(val):
                raise ValueError("numeric result must be a finite JSON number")
            converted["value"] = val
        else:
            reason = obj.get("reason")
            if type(reason) is not str or len(reason.strip()) < 12:
                raise ValueError("abstain requires meaningful reason")
            converted["reason"] = reason
        return converted, None
    except (json.JSONDecodeError, ValueError, TypeError) as e:
        error = str(e)
        return reject(task, error), error


def sha_file(filename):
    h = hashlib.sha256()
    with open(filename, "rb") as f:
        for part in iter(lambda: f.read(1 << 20), b""):
            h.update(part)
    return h.hexdigest()


def run(args):
    cards = cards_from(args.cards)
    master, selected = choose_tasks(cards)
    expected_ids = [t["task_id"] for t in selected]
    if len(set(expected_ids)) != 24:
        raise ValueError("duplicate selected task identity")
    all_cards = {c["id"]: c for c in cards["cards"]}
    meta = {
        "experiment": V4, "status": "ACTUAL_MODEL_RESPONSES",
        "model_name": args.model,
        "model_sha256": sha_file(args.model_file),
        "server_git_commit": args.server_commit,
        "runner_git_commit": os.environ.get("GITHUB_SHA", "LOCAL_UNVERIFIED"),
        "full_challenge_sha256": sha(master),
        "sample_task_ids": expected_ids,
        "sample_selection": "first_8_tasks_per_domain_in_original_generator_order",
        "generation": {"temperature": 0, "seed": 20261018,
                       "max_tokens": 120, "context": 4096,
                       "response_format": "json_object"},
        "arms": {}
    }
    for arm in ("A0", "A1"):
        records = []
        for index, task in enumerate(selected):
            prompt = build_prompt(task, all_cards[task["card_id"]], arm)
            started = time.monotonic()
            reply = call_server(args.url, args.model,
                                [{"role": "system", "content": SYSTEM},
                                 {"role": "user", "content": prompt}],
                                args.timeout)
            elapsed = time.monotonic() - started
            choices = reply.get("choices", [])
            if not choices or "message" not in choices[0]:
                raise RuntimeError("inference server returned no model message")
            raw = choices[0]["message"].get("content", "")
            if not isinstance(raw, str):
                raise RuntimeError("inference server returned nontext message")
            converted, error = adapt(raw, task)
            records.append({
                "task_id": task["task_id"],
                "arm": arm, "prompt": prompt, "system_prompt": SYSTEM,
                "raw_model_output": raw, "adapted_response": converted,
                "parser_error": error, "elapsed_seconds": elapsed,
                "usage": reply.get("usage", {}),
                "model_reported_by_server": reply.get("model", ""),
                "finish_reason": choices[0].get("finish_reason"),
            })
            print(f"{arm}: task={index+1:02d}/24 "
                  f"parse={'FAIL' if error else 'OK'} "
                  f"elapsed={elapsed:.2f}s", flush=True)
        meta["arms"][arm] = {
            "model_executed": True,
            "records": records,
            "parser_failure_count": sum(bool(r["parser_error"]) for r in records)
        }
        # Persist each completed arm before second arm; partial run evidence
        # must be retained even if subsequent requests fail.
        Path(args.output).write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n")
    return meta


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cards", required=True)
    p.add_argument("--model-file", required=True)
    p.add_argument("--model", default="qwen2.5-0.5b-instruct")
    p.add_argument("--server-commit", required=True)
    p.add_argument("--url", default="http://127.0.0.1:8080")
    p.add_argument("--timeout", type=int, default=180)
    p.add_argument("--output", required=True)
    args = p.parse_args()
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    report = run(args)
    print("f4_actual_model_inference=PASS arms=" + ",".join(report["arms"]))


if __name__ == "__main__":
    main()
