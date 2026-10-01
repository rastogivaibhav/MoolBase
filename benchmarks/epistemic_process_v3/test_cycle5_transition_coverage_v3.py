#!/usr/bin/env python3
"""Mutation tests for unscored native transition coverage; no oracle joins."""
import argparse, copy, json, subprocess, sys, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--raw',required=True)
p.add_argument('--blind-input',required=True)
a=p.parse_args()
raw=json.loads(Path(a.raw).read_text())
validator=Path(__file__).with_name('validate_cycle5_blind_v3.py')
mutations=[]
for cfg in ('G1','G2'):
    for kind in ('revision','decommitment','recommitment','resolution'):
        d=copy.deepcopy(raw)
        for ep in d['episodes']:
            if ep['configuration'] != cfg: continue
            for step in ep['steps']:
                step['native_events']=[e for e in step['native_events'] if e.get('type') != kind]
        mutations.append((cfg+':missing-'+kind,d))
d=copy.deepcopy(raw)
d['episodes'][0]['steps'][0]['native_events'].append({'type':'revision'})
mutations.append(('G0E:illegal-native-revision',d))
with tempfile.TemporaryDirectory() as td:
    for label,d in mutations:
        f=Path(td)/'mutated.json'
        f.write_text(json.dumps(d))
        r=subprocess.run([sys.executable,str(validator),'--raw',str(f),'--blind-input',a.blind_input],capture_output=True,text=True)
        assert r.returncode != 0, label
print(json.dumps({'transition_coverage_mutations_rejected':len(mutations),'score_bearing':False}))
