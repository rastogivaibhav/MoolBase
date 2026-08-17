#!/usr/bin/env python3
"""
REAL Microsoft CSuite benchmark against GrapheneDB.

No simulation. Every number comes from an actual subprocess call to the
real graphenedb_cli binary built from this repo's source.

What this tests, honestly:
  - CSuite ships the TRUE causal graph (adj_matrix.csv) for each dataset,
    plus observational/interventional data rows.
  - GrapheneDB is not a causal-discovery algorithm and does not do
    statistical ATE/CATE regression. It is a structured causal-memory
    store with a governed reasoning layer (causal_search / reason).
  - So the honest test is: given the TRUE graph loaded in as nodes+edges
    (one node per variable), can GrapheneDB's causal_search/reason
    correctly retrieve the causal ancestry of a variable, and does the
    governed `reason` pipeline (which requires multiple independent
    corroborating evidence paths) resolve on the datasets that have real
    branching/converging structure (e.g. large_backdoor's collider node)?

This is a test of causal-graph storage + retrieval + governed-reasoning
fidelity, not of causal discovery from raw data or of treatment-effect
estimation. Both capabilities and limits are reported explicitly.
"""

import csv
import json
import math
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

DIM = 32  # small graphs; keep vectors short


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


class RealCLI:
    def __init__(self, cli_path, db_path, dim):
        self.cli_path = cli_path
        self.db_path = db_path
        self.dim = dim
        self.call_count = 0

    def run(self, args, timeout=30):
        cmd = [self.cli_path] + args
        t0 = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        elapsed = time.time() - t0
        self.call_count += 1
        return result.returncode, result.stdout.strip(), result.stderr.strip(), elapsed

    def init(self):
        rc, out, err, _ = self.run(["init", self.db_path, str(self.dim)])
        if rc != 0:
            raise RuntimeError(f"init failed: {out} {err}")

    def put_node(self, content, vector, signature, role="root"):
        rc, out, err, elapsed = self.run(
            ["put-node", self.db_path, str(self.dim), content, vector, str(signature), role]
        )
        if rc != 0:
            raise RuntimeError(f"put-node failed: {out} {err}")
        return int(out.splitlines()[-1]), elapsed

    def put_edge(self, frm, to, role="causal"):
        rc, out, err, elapsed = self.run(
            ["put-edge", self.db_path, str(self.dim), str(frm), str(to), role]
        )
        if rc != 0:
            raise RuntimeError(f"put-edge failed: {out} {err}")
        return int(out.splitlines()[-1]), elapsed

    def search(self, vector, signature):
        return self.run(["search", self.db_path, str(self.dim), vector, str(signature)])

    def reason(self, vector, signature):
        return self.run(["reason", self.db_path, str(self.dim), vector, str(signature)])

    def neighbors(self, node_id, hops=1):
        return self.run(["neighbors", self.db_path, str(self.dim), str(node_id), str(hops)])

    def validate(self):
        return self.run(["validate", self.db_path, str(self.dim)])

    def inspect(self):
        return self.run(["inspect", self.db_path, str(self.dim)])


def parse_search_output(out):
    d = {"abstain": False, "reason": None, "target_node": None,
         "confidence": None, "paths": None, "why": []}
    for line in out.splitlines():
        if line.startswith("ABSTAIN"):
            d["abstain"] = True
            d["reason"] = line[len("ABSTAIN "):].strip()
        elif line.startswith("target="):
            parts = line.split()
            d["target_node"] = int(parts[0].split("=", 1)[1])
            d["confidence"] = float(parts[1].split("=", 1)[1])
            d["paths"] = int(parts[2].split("=", 1)[1])
        elif line.startswith("why="):
            d["why"].append(line.split("=", 1)[1])
    return d


def parse_reason_output(out):
    d = {"status": None, "primary_node": None, "confidence": None,
         "lyapunov_regime": None, "uncertainty": []}
    for line in out.splitlines():
        if line.startswith("status="):
            d["status"] = line.split("=", 1)[1]
        elif line.startswith("primary_node="):
            parts = line.split()
            d["primary_node"] = int(parts[0].split("=", 1)[1])
            d["confidence"] = float(parts[1].split("=", 1)[1])
        elif line.startswith("lyapunov_regime="):
            d["lyapunov_regime"] = line.split("=", 1)[1]
        elif line.startswith("uncertainty="):
            d["uncertainty"].append(line.split("=", 1)[1])
        elif line.startswith("ABSTAIN"):
            d["status"] = "abstain"
    return d


def load_adj_matrix(path):
    with open(path, encoding="utf-8") as f:
        rows = list(csv.reader(f))
    return [[int(float(x)) for x in row] for row in rows]


def load_variable_names(dataset_dir, n):
    vjson = dataset_dir / "variables.json"
    if vjson.exists():
        with open(vjson, encoding="utf-8") as f:
            meta = json.load(f)
        names = []
        lowers = []
        uppers = []
        types = []
        for v in meta.get("variables", []):
            names.append(v.get("group_name", v.get("name")))
            lowers.append(v.get("lower"))
            uppers.append(v.get("upper"))
            types.append(v.get("type"))
        if len(names) == n:
            return names, lowers, uppers, types
    return [f"x{i}" for i in range(n)], [None] * n, [None] * n, [None] * n


def ingest_dataset(cli, dataset_name, dataset_dir, results):
    print(f"\n{'='*78}")
    print(f"Dataset: {dataset_name}  ({dataset_dir})")
    print(f"{'='*78}")

    adj = load_adj_matrix(dataset_dir / "adj_matrix.csv")
    n = len(adj)
    names, lowers, uppers, types = load_variable_names(dataset_dir, n)

    true_edges = [(i, j) for i in range(n) for j in range(n) if adj[i][j] == 1]
    in_degree = [0] * n
    out_degree = [0] * n
    for i, j in true_edges:
        out_degree[i] += 1
        in_degree[j] += 1

    print(f"Variables: {n}, True edges: {len(true_edges)}")
    print(f"True edge list: {true_edges}")
    roots = [i for i in range(n) if in_degree[i] == 0]
    sinks = [i for i in range(n) if out_degree[i] == 0]
    print(f"Root variables (in_degree=0): {roots}")
    print(f"Sink variables (out_degree=0): {sinks}")
    # Colliders: nodes with >1 parent -- these are the real, honest test
    # of whether multiple independent causal paths can converge.
    colliders = [i for i in range(n) if in_degree[i] > 1]
    print(f"Collider variables (in_degree>1, real convergence): {colliders}")

    node_ids = {}
    for i in range(n):
        role = "root" if in_degree[i] == 0 else ("impact" if out_degree[i] == 0 else "symptom")
        desc_parts = [f"variable {names[i]}", dataset_name.replace("csuite_", "").replace("_", " ")]
        if types[i]:
            desc_parts.append(f"type {types[i]}")
        if lowers[i] is not None and uppers[i] is not None:
            desc_parts.append(f"range {lowers[i]:.2f} to {uppers[i]:.2f}")
        content = ", ".join(desc_parts)
        vec = embed(content)
        sig = abs(hash(f"{dataset_name}:{names[i]}")) % (2**31)
        node_id, _ = cli.put_node(content, vec, sig, role=role)
        node_ids[i] = node_id

    for i, j in true_edges:
        cli.put_edge(node_ids[i], node_ids[j], "causal")

    rc, out, err, _ = cli.validate()
    print(f"validate: rc={rc} {out!r}")

    # --- Real structural retrieval test ---
    # For each non-root variable, query with that variable's own real
    # description and check whether search/reason correctly identifies
    # ITS causal graph membership: does the traced path terminate at a
    # TRUE ancestor root of that variable (per adj_matrix), and for
    # collider variables, does `reason` (which requires multiple
    # independent evidence paths) actually resolve where a linear chain
    # would not?
    query_records = []
    for i in range(n):
        content = f"variable {names[i]}, {dataset_name.replace('csuite_', '').replace('_', ' ')}"
        vec = embed(content)
        sig = abs(hash(f"{dataset_name}:{names[i]}")) % (2**31)

        rc_s, out_s, err_s, elapsed_s = cli.search(vec, sig)
        sparsed = parse_search_output(out_s)

        rc_r, out_r, err_r, elapsed_r = cli.reason(vec, sig)
        rparsed = parse_reason_output(out_r)

        # ground truth: true ancestor roots of variable i (BFS backward
        # over the TRUE adjacency matrix, independent of what GrapheneDB
        # stored -- this is the actual causal ancestry from CSuite's
        # published graph)
        true_ancestors = set()
        frontier = [i]
        seen = {i}
        while frontier:
            cur = frontier.pop()
            for p in range(n):
                if adj[p][cur] == 1 and p not in seen:
                    seen.add(p)
                    true_ancestors.add(p)
                    frontier.append(p)
        true_root_ancestors = {a for a in true_ancestors if in_degree[a] == 0}
        if in_degree[i] == 0:
            true_root_ancestors = {i}  # i is its own root

        search_target_var = None
        for var_idx, nid in node_ids.items():
            if nid == sparsed["target_node"]:
                search_target_var = var_idx
        search_correct_root = (search_target_var in true_root_ancestors) if search_target_var is not None else False

        reason_target_var = None
        for var_idx, nid in node_ids.items():
            if nid == rparsed["primary_node"]:
                reason_target_var = var_idx
        reason_resolved = rparsed["status"] not in (None, "evidence_required", "abstain")
        reason_correct_root = reason_resolved and (reason_target_var in true_root_ancestors)

        query_records.append({
            "dataset": dataset_name,
            "variable_index": i,
            "variable_name": names[i],
            "in_degree": in_degree[i],
            "is_collider": in_degree[i] > 1,
            "true_root_ancestors": sorted(true_root_ancestors),
            "search_abstain": sparsed["abstain"],
            "search_abstain_reason": sparsed["reason"],
            "search_target_var": search_target_var,
            "search_confidence": sparsed["confidence"],
            "search_paths": sparsed["paths"],
            "search_correct_root": search_correct_root,
            "reason_status": rparsed["status"],
            "reason_resolved": reason_resolved,
            "reason_target_var": reason_target_var,
            "reason_confidence": rparsed["confidence"],
            "reason_correct_root": reason_correct_root,
        })

    n_colliders = sum(1 for r in query_records if r["is_collider"])
    n_search_correct = sum(1 for r in query_records if r["search_correct_root"])
    n_reason_resolved = sum(1 for r in query_records if r["reason_resolved"])
    n_reason_resolved_on_colliders = sum(1 for r in query_records if r["is_collider"] and r["reason_resolved"])

    print(f"\nStructural retrieval results ({n} variables, {n_colliders} colliders):")
    print(f"  search: correct-root retrieval on {n_search_correct}/{n} variables")
    print(f"  reason: resolved (non-abstain) on {n_reason_resolved}/{n} variables overall")
    print(f"  reason: resolved on {n_reason_resolved_on_colliders}/{n_colliders} collider variables "
          f"(these have >1 real independent causal parent)")
    for r in query_records:
        print(f"    var {r['variable_name']:<8} in_degree={r['in_degree']} "
              f"search_target={r['search_target_var']} search_correct_root={r['search_correct_root']} "
              f"reason_status={r['reason_status']}")

    results[dataset_name] = {
        "n_variables": n,
        "n_true_edges": len(true_edges),
        "n_colliders": n_colliders,
        "search_correct_root_rate": n_search_correct / n if n else None,
        "reason_resolved_rate": n_reason_resolved / n if n else None,
        "reason_resolved_rate_on_colliders": (n_reason_resolved_on_colliders / n_colliders) if n_colliders else None,
        "query_records": query_records,
    }


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", default="/work/build/graphenedb_cli")
    ap.add_argument("--data-dir", default="/work/benchmarks/csuite/data")
    ap.add_argument("--results-dir", default="/work/benchmarks/csuite/results")
    args = ap.parse_args()

    results_dir = Path(args.results_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 78)
    print("REAL Microsoft CSuite Benchmark -- actual graphenedb_cli subprocess calls")
    print("=" * 78)
    print(f"CLI binary: {args.cli}")
    print("Scope: causal-graph storage + retrieval + governed-reasoning fidelity.")
    print("NOT tested: causal discovery from raw data, ATE/CATE statistical")
    print("estimation -- GrapheneDB's CLI does not compute either; this benchmark")
    print("does not claim numbers for them.")

    data_dir = Path(args.data_dir)
    datasets = sorted(d for d in data_dir.iterdir() if d.is_dir() and (d / "adj_matrix.csv").exists())
    print(f"\nFound {len(datasets)} real CSuite dataset(s): {[d.name for d in datasets]}")

    results = {}
    for dataset_dir in datasets:
        db_path = f"/tmp/csuite_{dataset_dir.name}.db"
        if Path(db_path).exists():
            shutil.rmtree(db_path)
        cli = RealCLI(args.cli, db_path, DIM)
        cli.init()
        ingest_dataset(cli, dataset_dir.name, dataset_dir, results)

    print(f"\n{'='*78}")
    print("SUMMARY (real, across all datasets)")
    print(f"{'='*78}")
    for name, r in results.items():
        print(f"\n{name}: {r['n_variables']} vars, {r['n_true_edges']} edges, {r['n_colliders']} colliders")
        print(f"  search correct-root rate:              {r['search_correct_root_rate']:.1%}")
        print(f"  reason resolved rate (all variables):  {r['reason_resolved_rate']:.1%}")
        if r["reason_resolved_rate_on_colliders"] is not None:
            print(f"  reason resolved rate (colliders only): {r['reason_resolved_rate_on_colliders']:.1%}")
        else:
            print(f"  reason resolved rate (colliders only): N/A (no colliders in this graph)")

    out_path = results_dir / "real_csuite_results.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print(f"\nFull results written to: {out_path}")


if __name__ == "__main__":
    main()
