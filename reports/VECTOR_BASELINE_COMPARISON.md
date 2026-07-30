# Vector Baseline Comparison

## Purpose

`graphenedb_vector_baseline_bench` compares GrapheneDB causal retrieval against vector-only top-1 retrieval on the same synthetic incident dataset.

Each incident has:

- one root-cause node
- one vector-similar distractor node
- one symptom node
- one causal edge from root to symptom

The vector-only baseline asks whether top-1 vector search returns the root cause. The causal path asks whether `causal_search()` returns the incident root.

## Commands

Windows:

```powershell
.\scripts\run_vector_baseline_bench.ps1 -Incidents 5000 -Queries 200 -Dim 64
```

POSIX:

```bash
INCIDENTS=5000 QUERIES=200 DIM=64 scripts/run_vector_baseline_bench.sh
```

The scripts write:

```text
reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt
```

## Local Smoke Evidence

Windows smoke command run in this workspace:

```powershell
.\scripts\run_vector_baseline_bench.ps1 -Incidents 500 -Queries 50 -Dim 32 -BuildDir build-release -Out reports\VECTOR_BASELINE_COMPARISON_OUTPUT.txt
```

Observed output:

```text
vector_baseline_comparison=true
incidents=500 nodes=1500 edges=500 dim=32 queries=50
vector_root_hit_rate=0
causal_root_hit_rate=1
```

## Metrics

- `vector_root_hit_rate`: fraction of queries where vector-only top-1 returned the root cause.
- `causal_root_hit_rate`: fraction of queries where causal retrieval returned the root cause.
- `vector_p95_ms`: vector-only top-1 p95 latency.
- `causal_p95_ms`: causal retrieval p95 latency.

## Interpretation

This benchmark is a behavioral comparison, not a production performance certificate. It is intended to show why a graph/causal memory layer exists: vector similarity can prefer a near-textual distractor, while causal retrieval can follow evidence edges back to the root memory.

Enterprise GA still requires larger target-hardware runs, production embedding dimensions, cold-cache and warm-cache profiles, and persisted ANN index integration.
