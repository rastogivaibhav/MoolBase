#!/usr/bin/env python3
"""Compare two Cycle-4 unscored runs after removing timing-only values."""
from __future__ import annotations

import json
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any


def scrub(value: Any) -> Any:
    if isinstance(value, dict):
        return {
            key: scrub(item)
            for key, item in value.items()
            if key not in {"execution_seconds"}
        }
    if isinstance(value, list):
        return [scrub(item) for item in value]
    return value


if len(sys.argv) != 3:
    raise SystemExit("usage: compare_cycle4_determinism.py <left> <right>")

left = scrub(json.loads(Path(sys.argv[1]).read_text(encoding="utf-8")))
right = scrub(json.loads(Path(sys.argv[2]).read_text(encoding="utf-8")))
if left != right:
    raise SystemExit("cycle4 semantic determinism check failed")
print("cycle4_semantic_determinism=passed")
