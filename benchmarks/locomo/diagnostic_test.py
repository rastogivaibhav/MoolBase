#!/usr/bin/env python3
"""Diagnostic: does reason()/search() ever resolve on this real DB, under
favorable conditions (querying with near-identical text to a stored node)?
This isolates whether abstention is due to weak question-vs-message lexical
overlap, versus the pipeline being broken outright.
"""
import json
import re
import math
import subprocess
import sys

DIM = 64

def tokenize(text):
    return re.findall(r"[a-z0-9]+", text.lower())

def embed(text, dim=DIM):
    vec = [0.0] * dim
    for tok in tokenize(text):
        h = hash(tok) % dim
        vec[h] += 1.0
    norm = math.sqrt(sum(v * v for v in vec)) or 1.0
    vec = [v / norm for v in vec]
    vec = [v if v != 0.0 else 1e-6 for v in vec]
    return ",".join(f"{v:.6f}" for v in vec)

CLI = "/work/build/graphenedb_cli"
DB = "/tmp/diag.db"

def run(args):
    r = subprocess.run([CLI] + args, capture_output=True, text=True, timeout=30)
    return r.returncode, r.stdout.strip(), r.stderr.strip()

import shutil, os
if os.path.exists(DB):
    shutil.rmtree(DB)

rc, out, err = run(["init", DB, str(DIM)])
print("init:", rc, out, err)

texts = [
    "Caroline went to the LGBTQ support group on 7 May 2023",
    "Melanie painted a sunrise over the lake last weekend",
    "The weather today is sunny with a light breeze",
]
node_ids = []
for i, t in enumerate(texts):
    v = embed(t)
    sig = abs(hash(f"node{i}")) % (2**31)
    rc, out, err = run(["put-node", DB, str(DIM), t, v, str(sig), "root"])
    print(f"put-node {i}: rc={rc} out={out} err={err}")
    node_ids.append(int(out))

for i in range(len(node_ids) - 1):
    rc, out, err = run(["put-edge", DB, str(DIM), str(node_ids[i]), str(node_ids[i+1]), "causal"])
    print(f"put-edge {i}: rc={rc} out={out} err={err}")

print("\n--- Query 1: EXACT same text as node 0 (best case) ---")
v = embed(texts[0])
sig = abs(hash(f"node0")) % (2**31)  # same signature as node 0 too
rc, out, err = run(["search", DB, str(DIM), v, str(sig)])
print("search rc=", rc)
print(out)
print(err)

rc, out, err = run(["reason", DB, str(DIM), v, str(sig)])
print("\nreason rc=", rc)
print(out)
print(err)

print("\n--- Query 2: paraphrased question about node 0's content (realistic case) ---")
q = "When did Caroline go to the LGBTQ support group?"
v2 = embed(q)
sig2 = abs(hash(q)) % (2**31)
rc, out, err = run(["search", DB, str(DIM), v2, str(sig2)])
print("search rc=", rc)
print(out)
print(err)

print("\n--- cosine similarity check (python side) ---")
def parse_vec(s):
    return [float(x) for x in s.split(",")]

def cosine(a, b):
    dot = sum(x*y for x, y in zip(a, b))
    na = math.sqrt(sum(x*x for x in a))
    nb = math.sqrt(sum(x*x for x in b))
    return dot / (na * nb) if na and nb else 0.0

va = parse_vec(v)
vb = parse_vec(v2)
print(f"cosine(exact-text-vec, paraphrase-question-vec) = {cosine(va, vb):.4f}")
