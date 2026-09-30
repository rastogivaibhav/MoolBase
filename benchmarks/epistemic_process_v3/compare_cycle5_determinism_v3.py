#!/usr/bin/env python3
"""Exact semantic replay comparison for V3 Cycle-5 blind runs."""
from __future__ import annotations
import json,sys
from pathlib import Path
if len(sys.argv)!=3:
    raise SystemExit("usage: compare_cycle5_determinism_v3.py <left> <right>")
left=json.loads(Path(sys.argv[1]).read_text())
right=json.loads(Path(sys.argv[2]).read_text())
if left!=right:
    raise SystemExit("v3 Cycle-5 semantic determinism failed")
print("v3_cycle5_semantic_determinism=passed")
