"""Replay LF/CRLF and reject real manifest changes using the compiled flagship."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from run_flagship_proof import ROOT, EXPECTED_MECHANISM_RECEIPT_SHA256

p = argparse.ArgumentParser()
p.add_argument('--build-dir', default='build-flagship-proof')
p.add_argument('--output-dir', default='reports/flagship-checkout')
a = p.parse_args()
manifest = ROOT / 'benchmarks/flagship/scenario.json'
original = manifest.read_bytes()
lf = original.replace(b'\r\n', b'\n')
out = Path(a.output_dir)
if not out.is_absolute():
    out = ROOT / out
out.mkdir(parents=True, exist_ok=True)
results = {}
try:
    for name, data in [('lf', lf), ('crlf', lf.replace(b'\n', b'\r\n')),
                       ('mutated', lf.replace(b'known-lineage de-correlation', b'fabricated independence'))]:
        if name == 'mutated' and data == lf:
            raise RuntimeError('Mutation did not change fixture')
        manifest.write_bytes(data)
        run = subprocess.run([sys.executable, str(ROOT/'scripts/run_flagship_proof.py'),
            '--skip-build', '--build-dir', a.build_dir, '--output-dir', str(out/name)],
            cwd=ROOT, text=True, capture_output=True)
        (out/(name+'.stdout.txt')).write_text(run.stdout, encoding='utf-8')
        (out/(name+'.stderr.txt')).write_text(run.stderr, encoding='utf-8')
        if name == 'mutated':
            if run.returncode == 0 or 'mechanism receipt changed' not in run.stderr:
                raise RuntimeError('Changed manifest did not fail the frozen contract')
            results[name] = 'rejected'
        else:
            if run.returncode:
                raise RuntimeError(run.stdout + run.stderr)
            receipt = json.loads((out/name/'receipt.json').read_text(encoding='utf-8'))
            if receipt['receipt_hash_sha256'] != EXPECTED_MECHANISM_RECEIPT_SHA256:
                raise RuntimeError('Frozen receipt identity changed')
            results[name] = receipt['receipt_hash_sha256']
finally:
    manifest.write_bytes(original)
(out/'checkout-results.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
print('PASS: LF and CRLF reproduce the existing frozen receipt; changed manifest fails closed')
