#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json
from pathlib import Path
import generate_tasks_v3 as gen

ROOT=Path(__file__).resolve().parent
committed=(ROOT/"tasks_v3_candidate.json").read_bytes()
a=gen.canonical_bytes(gen.generate())
b=gen.canonical_bytes(gen.generate())
assert a==b==committed
assert hashlib.sha256(a).hexdigest()==hashlib.sha256(committed).hexdigest()
forward=gen.generate(gen.FAMILIES)
reverse=gen.generate(list(reversed(gen.FAMILIES)))
fm={e["id"]:e for e in forward["episodes"]}
rm={e["id"]:e for e in reverse["episodes"]}
assert fm.keys()==rm.keys()
for key in fm: assert fm[key]==rm[key],key
print(f"cycle3_determinism=passed sha256={hashlib.sha256(committed).hexdigest()} family_order_independent=true")
