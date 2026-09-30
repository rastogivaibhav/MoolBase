#!/usr/bin/env python3
"""Compare V2 raw artifacts for semantic determinism while excluding wall-clock cost."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

VOLATILE={"execution_seconds"}

def scrub(value):
    if isinstance(value,dict):
        return {k:scrub(v) for k,v in sorted(value.items()) if k not in VOLATILE}
    if isinstance(value,list):
        return [scrub(v) for v in value]
    return value

def digest(path):
    obj=scrub(json.loads(Path(path).read_text(encoding="utf-8")))
    data=json.dumps(obj,sort_keys=True,separators=(",",":")).encode()
    return hashlib.sha256(data).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("left"); ap.add_argument("right")
    args=ap.parse_args()
    a=digest(args.left); b=digest(args.right)
    print(json.dumps({"left":a,"right":b,"semantic_determinism":a==b},sort_keys=True))
    if a!=b: raise SystemExit("semantic determinism check failed")

if __name__=="__main__":
    main()
