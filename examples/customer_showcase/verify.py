#!/usr/bin/env python3
"""Bounded checks for actual customer replay contracts, not a release gate."""
import json, subprocess, tempfile, sys
from pathlib import Path
binary=Path(sys.argv[1] if len(sys.argv)>1 else 'build-showcase/moolbase_customer_showcase').resolve()
scenarios=json.loads(Path(__file__).with_name('scenarios.json').read_text())
executions=0

def replay(s):
 global executions
 with tempfile.TemporaryDirectory() as temp:
  tsv=Path(temp)/'events.tsv'
  tsv.write_text('\n'.join('\t'.join([e['id'],e['content'],e['family'],str(e['target']),e['kind'],str(int(e['verified'])),e['retire'],'']) for e in s['events'])+'\n')
  p=subprocess.run([str(binary),str(Path(temp)/'database'),str(tsv),*s['hypotheses']],text=True,capture_output=True)
  executions+=1
  states=[json.loads(x) for x in p.stdout.splitlines()]
  return p,states
for s in scenarios:
 p,states=replay(s);assert p.returncode==0,p.stderr
 assert states[-1]['bundleHash']==states[-2]['bundleHash']
 assert states[-1]['answer']==2
 assert states[-1]['status']==('resolved' if s['id']=='memory' else 'provisionally_resolved')
 # Rejected alternatives must not contest the replacement; direct opposition
 # to the initially selected explanation still produces a contested state.
 if s['id'] != 'memory':
  assert any(x['status']=='contested' for x in states)
  assert any(e['type']=='challenge' and e['to']==1 for x in states for e in x['events'])
 assert all(x['receipt']['coreExecuted'] and x['receipt']['noSilentPromotion'] for x in states)
 assert states[1]['targets'][0]['families']==states[2]['targets'][0]['families'] if s['id']!='memory' else states[2]['targets'][0]['families']==states[3]['targets'][0]['families']
 events={e['type'] for x in states for e in x['events']}
 assert {'revision','decommitment'}<=events
 if s['id']=='memory':assert {'recommitment','resolution'}<=events
 # Different wording never changes vector/provenance-based outcomes.
 variant=json.loads(json.dumps(s))
 for e in variant['events']:e['content']='Quoted "source" with \\ escape and UTF-8: café — '+e['content']
 p,changed=replay(variant);assert p.returncode==0
 assert [(x['answer'],x['status'],[t['families'] for t in x['targets']]) for x in changed]==[(x['answer'],x['status'],[t['families'] for t in x['targets']]) for x in states]
 # Replay is reproducible from a second fresh native database.
 p,again=replay(s);assert p.returncode==0
 assert [(x['bundleHash'],x['answer'],x['status']) for x in again]==[(x['bundleHash'],x['answer'],x['status']) for x in states]
# Rejected retirement references cannot silently insert replacement evidence.
s=json.loads(json.dumps(scenarios[0]));s['events']=[dict(s['events'][0],kind='revoke',retire='missing')]
p,states=replay(s);assert p.returncode==1 and not states[-1]['ok']
# CLI must refuse a pre-existing database path.
with tempfile.TemporaryDirectory() as temp:
 p=subprocess.run([str(binary),temp,'missing.tsv','A','B'],capture_output=True,text=True)
 assert p.returncode==2 and 'never overwritten' in p.stderr
print(f'PASS: {executions} native replay executions; duplicate families, lifecycle, native transitions, JSON escaping, determinism, reopen and rejected input.')
