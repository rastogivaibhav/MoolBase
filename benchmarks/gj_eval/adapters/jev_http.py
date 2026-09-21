#!/usr/bin/env python3
"""Official TypeSafe/Jev HTTP adapter for GJ-Eval v1.

Reads one GJ-Eval task from stdin and emits the provider decision JSON expected
by run_command_adapter.py.

Environment:
  TYPESAFE_API_KEY              required for live calls
  TYPESAFE_API_URL              default https://api.typesafe.ai/v1/systemone
  JEV_MODEL                     default jev-latest
  GJ_JEV_ACTION_THRESHOLD       default 0.75 (pre-registered)
  GJ_JEV_INPUT_USD_PER_MTOK     default 0.042

The implementation intentionally uses the documented HTTP contract instead of
guessing an unstable/private SDK surface.
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from typing import Any


DEFAULT_URL = "https://api.typesafe.ai/v1/systemone"
DEFAULT_MODEL = "jev-1.13.0"
DEFAULT_ACTION_THRESHOLD = 0.75
DEFAULT_INPUT_USD_PER_MTOK = 0.042


def root_criteria(task: dict[str, Any]) -> dict[str, str]:
    criteria: dict[str, str] = {}
    for choice in task.get("choices", []):
        cid = str(choice["id"])
        label = str(choice.get("label") or cid)
        if cid == "unknown":
            criteria[cid] = (
                "The currently visible evidence is insufficient to identify "
                "one primary cause reliably."
            )
        else:
            criteria[cid] = (
                f"{label} is the primary causal explanation of the observed "
                "checkout failures, rather than merely correlated in time."
            )
    return criteria


def test_criteria(task: dict[str, Any]) -> dict[str, str]:
    criteria = {
        str(item["id"]): str(item.get("label") or item["id"])
        for item in task.get("tests", [])
    }
    criteria["none"] = (
        "No additional listed test is currently useful or needed to reduce "
        "decision uncertainty."
    )
    return criteria


def normalize_state(task: dict[str, Any]) -> str | list[Any] | dict[str, Any]:
    visible = task.get("visible_state", [])
    if visible and all(isinstance(item, str) for item in visible):
        # J0 raw arm: intentionally remove canonical provenance structure.
        return "\n".join(str(item) for item in visible)
    return {
        "domain": task.get("domain"),
        "current_timestep": task.get("timestep"),
        "observations": visible,
    }


def build_request(task: dict[str, Any]) -> dict[str, Any]:
    questions: dict[str, Any] = {
        "root_cause": {
            "type": "choice",
            "instructions": (
                "Which option is the primary causal explanation of the observed "
                "checkout failures given only the currently visible evidence?"
            ),
            "criteria": root_criteria(task),
        },
        "evidence_sufficient": {
            "type": "noul",
            "instructions": (
                "Is the currently visible evidence sufficient to identify the "
                "primary cause reliably enough for an automated system to act "
                "without human review?"
            ),
            "criteria": {
                "true": (
                    "The evidence is sufficiently discriminating and internally "
                    "consistent to justify automated action."
                ),
                "false": (
                    "Important ambiguity, contradiction, dependence, or missing "
                    "evidence remains; review or more evidence is appropriate."
                ),
            },
        },
    }
    if task.get("tests"):
        questions["next_test"] = {
            "type": "choice",
            "instructions": (
                "If more evidence would materially reduce uncertainty about the "
                "primary cause, which single listed test should be run next?"
            ),
            "criteria": test_criteria(task),
        }

    return {
        "state": normalize_state(task),
        "model": os.environ.get("JEV_MODEL", DEFAULT_MODEL),
        "questions": questions,
    }


def call_typesafe(payload: dict[str, Any]) -> tuple[dict[str, Any], int]:
    api_key = os.environ.get("TYPESAFE_API_KEY", "")
    if not api_key:
        raise RuntimeError("TYPESAFE_API_KEY is required for live Jev calls")
    url = os.environ.get("TYPESAFE_API_URL", DEFAULT_URL)
    body = json.dumps(payload, sort_keys=True).encode("utf-8")
    max_retries = 5

    for attempt in range(max_retries + 1):
        req = urllib.request.Request(
            url,
            data=body,
            headers={
                "Authorization": f"Bearer {api_key}",
                "Content-Type": "application/json",
                "Accept": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                raw = response.read().decode("utf-8")
                parsed = json.loads(raw) if raw else {}
                if response.status < 200 or response.status >= 300:
                    raise RuntimeError(
                        f"TypeSafe returned HTTP {response.status}: {parsed}"
                    )
                return parsed, attempt
        except urllib.error.HTTPError as exc:
            raw = exc.read().decode("utf-8", errors="replace")
            if exc.code in (429, 529) and attempt < max_retries:
                retry_after = exc.headers.get("Retry-After")
                try:
                    delay = float(retry_after) if retry_after else 0.5 * (2 ** attempt)
                except ValueError:
                    delay = 0.5 * (2 ** attempt)
                time.sleep(min(8.0, max(0.1, delay)))
                continue
            raise RuntimeError(
                f"TypeSafe returned HTTP {exc.code}: {raw}"
            ) from exc

    raise RuntimeError("TypeSafe retry loop exhausted")


def parse_response(response: dict[str, Any]) -> dict[str, Any]:
    answers = response.get("answers")
    if not isinstance(answers, dict):
        raise ValueError("TypeSafe response missing answers object")

    root = answers.get("root_cause")
    sufficient = answers.get("evidence_sufficient")
    if not isinstance(root, dict) or root.get("type") != "choice":
        raise ValueError("TypeSafe response missing root_cause Choice answer")
    if not isinstance(sufficient, dict) or sufficient.get("type") != "noul":
        raise ValueError("TypeSafe response missing evidence_sufficient Noul answer")

    choice = str(root.get("choice"))
    probabilities = {
        str(k): float(v) for k, v in dict(root.get("probabilities") or {}).items()
    }
    if choice not in probabilities:
        raise ValueError("root_cause choice absent from probabilities")
    selected_probability = probabilities[choice]
    native_confidence = float(root.get("confidence", 0.0))
    sufficient_probability = float(sufficient.get("noul", 0.0))
    threshold = float(
        os.environ.get("GJ_JEV_ACTION_THRESHOLD", DEFAULT_ACTION_THRESHOLD)
    )

    act = (
        "act"
        if choice != "unknown"
        and selected_probability >= threshold
        and sufficient_probability >= threshold
        else "review"
    )

    ranked = [
        key
        for key, _ in sorted(
            probabilities.items(), key=lambda item: (-item[1], item[0])
        )
        if key != "unknown"
    ]

    requested_test = None
    next_test = answers.get("next_test")
    if act != "act" and isinstance(next_test, dict):
        candidate = next_test.get("choice")
        if candidate and candidate != "none":
            requested_test = str(candidate)

    usage = response.get("usage") if isinstance(response.get("usage"), dict) else {}
    input_tokens = int(usage.get("input_tokens", 0) or 0)
    price = float(
        os.environ.get(
            "GJ_JEV_INPUT_USD_PER_MTOK", DEFAULT_INPUT_USD_PER_MTOK
        )
    )

    return {
        "root_choice": choice,
        "act": act,
        # GJ-Eval calibration uses the selected outcome probability. Jev's
        # native confidence statistic is retained separately below.
        "selected_confidence": selected_probability,
        "choice_probabilities": probabilities,
        "ranked_hypotheses": ranked,
        "requested_test": requested_test,
        "epistemic_status": (
            "supported" if act == "act" else "provisional"
        ),
        "provider_cost": input_tokens * price / 1_000_000.0,
        "receipt": {
            "provider": "typesafe",
            "model": response.get("model"),
            "jev_native_confidence": native_confidence,
            "evidence_sufficient_noul": sufficient_probability,
            "action_threshold": threshold,
            "usage": usage,
        },
    }


def main() -> int:
    task = json.load(sys.stdin)
    payload = build_request(task)
    response, retries = call_typesafe(payload)
    output = parse_response(response)
    receipt = output.setdefault("receipt", {})
    receipt["transport_retries"] = retries
    receipt["state_sha256"] = hashlib.sha256(
        json.dumps(payload["state"], sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    receipt["questions_sha256"] = hashlib.sha256(
        json.dumps(payload["questions"], sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    print(json.dumps(output, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
