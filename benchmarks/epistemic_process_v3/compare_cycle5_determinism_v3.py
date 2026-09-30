#!/usr/bin/env python3
from __future__ import annotations
import argparse, copy, json
from pathlib import Path

def normalise(raw):
    x=copy.deepcopy(raw)
    # No wall-clock timings are written by the V3 harness. Keep an explicit
    # sanitizer for inherited nullable execution fields.
    for ep in x.get("episodes",[]):
        ep.pop("execution_seconds",None)
        for step in ep.get("steps",[]):
            if isinstance(step.get("execution"),dict):
                step["execution"]["execution_seconds"]=None
    return x

def main():
    p=argparse.ArgumentParser()
    p.add_argument("left"); p.add_argument("right")
    args=p.parse_args()
    a=normalise(json.loads(Path(args.left).read_text()))
    b=normalise(json.loads(Path(args.right).read_text()))
    assert a==b,"blind mechanical runs are not deterministic"
    print(json.dumps({"cycle5_determinism":"passed","executions":len(a["episodes"])},sort_keys=True))
if __name__=="__main__": main()
