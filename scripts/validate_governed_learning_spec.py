#!/usr/bin/env python3
"""Validate the governed-learning specification's executable traceability."""

from __future__ import annotations

import pathlib
import re
import sys

REQUIRED_SECTIONS = {
    "## Status and scope",
    "## System boundary",
    "## Epistemic and control invariants",
    "## Learning episode",
    "## Policy learning",
    "## Data-use decisions",
    "## API surface",
    "## Requirements and verification trace",
    "## Acceptance for GDB-GL-0",
}
REQUIREMENT = re.compile(r"^\| (GL-(\d{3})) \| (.+?) \| (.+?) \|$")


def validate(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    failures: list[str] = []
    for section in sorted(REQUIRED_SECTIONS):
        if section not in lines:
            failures.append(f"missing section: {section}")

    requirements: dict[str, tuple[str, str]] = {}
    numbers: list[int] = []
    for line in lines:
        match = REQUIREMENT.match(line)
        if not match:
            continue
        requirement_id, number, statement, evidence = match.groups()
        if requirement_id in requirements:
            failures.append(f"duplicate requirement: {requirement_id}")
        requirements[requirement_id] = (statement, evidence)
        numbers.append(int(number))
        if not statement.endswith("."):
            failures.append(f"{requirement_id} statement must end with a period")
        if not any(
            marker in evidence
            for marker in ("tests/", "scripts/", "CTest", "Docker", "docs/")
        ):
            failures.append(f"{requirement_id} has no executable evidence")

    expected = list(range(1, 23))
    if sorted(numbers) != expected:
        failures.append(
            f"requirement sequence must be GL-001..GL-022, got {sorted(numbers)}"
        )

    required_phrases = (
        "does not train or mutate LLM weights",
        "Evaluation episodes",
        "explicit approval",
        "no new record type",
        "does not claim cryptographic tenant isolation",
        "does not pass",
    )
    for phrase in required_phrases:
        if phrase not in text:
            failures.append(f"missing safety/scope phrase: {phrase}")
    return failures


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: validate_governed_learning_spec.py <spec>", file=sys.stderr)
        return 2
    path = pathlib.Path(sys.argv[1])
    failures = validate(path)
    if failures:
        for failure in failures:
            print(f"FAIL {failure}", file=sys.stderr)
        return 1
    print("governed_learning_spec_valid=true requirements=22")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
