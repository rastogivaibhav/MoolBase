#!/usr/bin/env python3
"""Run evidence correction through the native C++ adapter; no model/API key."""
import argparse,json,subprocess,tempfile
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,default=Path('build-showcase/moolbase_customer_showcase'));p.add_argument('--output',type=Path,default=Path('reports/customer-showcase/python-memory.json'));a=p.parse_args()
 s=next(s for s in json.loads(Path(__file__).with_name('scenarios.json').read_text()) if s['id']=='memory')
 with tempfile.TemporaryDirectory(prefix='moolbase-memory-') as temp:
  tsv=Path(temp)/'events.tsv';tsv.write_text('\n'.join('\t'.join([e['id'],e['content'],e['family'],str(e['target']),e['kind'],str(int(e['verified'])),e['retire']]) for e in s['events'])+'\n')
  r=subprocess.run([str(a.binary.resolve()),str(Path(temp)/'db'),str(tsv),*s['hypotheses']],text=True,capture_output=True)
  if r.returncode:raise RuntimeError(r.stderr or r.stdout)
  states=[json.loads(line) for line in r.stdout.splitlines()]
  if not all(x['ok'] for x in states):raise RuntimeError('Engine rejected an event')
  corrected=states[4];old=next(e for e in corrected['evidence'] if e['id']=='old-profile')
  if old['state']!='superseded':raise RuntimeError('Original evidence was not retired')
  if states[-1]['bundleHash']!=states[-2]['bundleHash']:raise RuntimeError('Reopen changed bundle')
  a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'scenario':'memory','persistence':'temporary native files; prior reasoning state stays in this adapter process','states':states},indent=2)+'\n')
  for label,index in [('Before correction',3),('After supersession',4),('After independent corroboration and retiring stale copies',8),('After file reopen',9)]:
   x=states[index];target=next((t['content'] for t in x['targets'] if t['id']==x['answer']),'No operative answer');print(f'{label}: {x["status"]} | {target}')
  print(f'Receipt: {a.output}')
if __name__=='__main__':main()
