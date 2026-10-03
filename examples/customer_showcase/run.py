#!/usr/bin/env python3
"""Replay realistic synthetic fixtures through the native MoolBase engine."""
import argparse, json, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=Path('build-showcase/moolbase_customer_showcase'));p.add_argument('--output',type=Path,default=Path('reports/customer-showcase'));a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
scenarios=json.loads(Path(__file__).with_name('scenarios.json').read_text())
for s in scenarios:
 with tempfile.TemporaryDirectory(prefix='moolbase-customer-') as temp:
  tsv=Path(temp)/'events.tsv'
  tsv.write_text('\n'.join('\t'.join([e['id'],e['content'],e['family'],str(e['target']),e['kind'],str(int(e['verified'])),e['retire'],'']) for e in s['events'])+'\n')
  r=subprocess.run([str(a.binary.resolve()),str(Path(temp)/'database'),str(tsv),*s['hypotheses']],text=True,capture_output=True,check=True)
  states=[json.loads(line) for line in r.stdout.splitlines()]
  assert all(x['ok'] and x['receipt']['coreExecuted'] and x['receipt']['noSilentPromotion'] for x in states)
  assert states[-1]['bundleHash']==states[-2]['bundleHash'], 'reopen changed evidence bundle'
  assert states[-1]['answer']==states[-2]['answer'], 'reopen changed answer'
  (a.output/(s['id']+'.json')).write_text(json.dumps(dict(scenario=s['id'],dataClass='realistic synthetic replay',states=states),indent=2)+'\n')
  print(s['id'], 'PASS', len(s['events']), 'events; reopen preserved bundle; final',states[-1]['status'],'answer',states[-1]['answer'])
