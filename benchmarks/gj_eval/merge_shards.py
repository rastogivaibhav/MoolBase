#!/usr/bin/env python3
"""Strictly merge execution shards without changing GJ-Eval semantics."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path


def rows(path: Path):
    return [json.loads(x) for x in path.read_text().splitlines() if x.strip()]


def sha256(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--worlds', required=True)
    p.add_argument('--shards', nargs='+', required=True)
    p.add_argument('--system', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--receipt', required=True)
    a=p.parse_args()
    wp=Path(a.worlds); worlds=rows(wp)
    canonical=[]
    for w in worlds:
        for step in w['timeline']:
            canonical.append((w['world_id'], int(step['timestep'])))
    expected=set(canonical)
    merged={}; shard_receipts=[]
    for name in a.shards:
        path=Path(name); rs=rows(path)
        shard_receipts.append({'path':str(path),'rows':len(rs),'sha256':sha256(path)})
        for r in rs:
            if r.get('system') != a.system:
                raise SystemExit(f"wrong system in {path}: {r.get('system')!r}")
            key=(r['world_id'], int(r['timestep']))
            if key not in expected:
                raise SystemExit(f'unexpected primary key: {key}')
            if key in merged:
                raise SystemExit(f'duplicate primary key: {key}')
            if r.get('adapter_status') != 'ok':
                raise SystemExit(f"adapter failure at {key}: {r.get('adapter_status')}")
            merged[key]=r
    missing=[k for k in canonical if k not in merged]
    if missing:
        raise SystemExit(f'missing {len(missing)} primary keys; first={missing[:5]}')
    out=Path(a.output); out.parent.mkdir(parents=True, exist_ok=True)
    with out.open('w') as h:
        for key in canonical:
            h.write(json.dumps(merged[key], sort_keys=True)+'\n')
    receipt={'schema_version':1,'system':a.system,'worlds_sha256':sha256(wp),'expected_rows':len(canonical),'merged_rows':len(merged),'output_sha256':sha256(out),'shards':shard_receipts,'coverage_complete':True,'adapter_failures':0}
    Path(a.receipt).write_text(json.dumps(receipt, indent=2, sort_keys=True)+'\n')
    print(json.dumps(receipt, sort_keys=True))

if __name__=='__main__': main()
