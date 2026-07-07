# Vector Index Recall Gate

GrapheneDB now has a benchmark gate that compares the selected vector index against exact flat search.

## Command

Windows:

```powershell
.\scripts\run_vector_index_recall_bench.ps1 -Nodes 5000 -Queries 200 -Dim 32 -K 10 -Index auto -MinRecall 0.999
```

POSIX:

```bash
NODES=5000 QUERIES=200 DIM=32 K=10 INDEX=auto MIN_RECALL=0.999 scripts/run_vector_index_recall_bench.sh
```

## What It Measures

- mean recall@k versus exact flat search
- worst recall@k
- exact p95 latency
- selected-index p95 latency
- requested and resolved vector index policy

## GA Use

This is the regression gate future ANN integrations must satisfy. FAISS, HNSW, or DiskANN should not be promoted to a GA vector index until this benchmark and the larger storage/retrieval profile pass on target hardware with preserved reports.

The current in-tree indexes remain in-memory and rebuilt on open. This gate makes that limitation explicit while giving the project a concrete acceptance test for replacing or extending the vector index.

## Latest Local Smoke

Windows smoke command:

```powershell
.\scripts\run_vector_index_recall_bench.ps1 -Nodes 1000 -Queries 40 -Dim 32 -K 10 -Index flat -MinRecall 1.0 -Out reports/VECTOR_INDEX_RECALL_OUTPUT.txt
```

Result:

```text
mean_recall_at_k=1
worst_recall_at_k=1
```

Additional `auto`/KD-tree probe:

```text
nodes=1000 queries=40 dim=16 k=10
candidate_requested=auto
vector_index_requested=auto
vector_index=kdtree
mean_recall_at_k=1
worst_recall_at_k=1
```
