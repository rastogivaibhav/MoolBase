# Vector Index Boundary

GrapheneDB has a pluggable vector-index boundary:

```cpp
class VectorIndex {
public:
  virtual Status add(uint32_t id, const std::vector<float>& vector) = 0;
  virtual Status remove(uint32_t id) = 0;
  virtual std::vector<SearchResult> search(const std::vector<float>& query, size_t k) const = 0;
  virtual const char* name() const = 0;
};
```

`DBOptions::vector_index_kind` selects the current index:

| Kind | Behavior |
|---|---|
| `VectorIndexKind::Auto` | Uses KD-tree for dimensions `<= 32`, flat index for higher-dimensional embeddings. |
| `VectorIndexKind::Flat` | Exact in-memory cosine scan. Good as the correctness baseline. |
| `VectorIndexKind::KDTree` | In-memory KD-tree over normalized vectors. Useful for small-dimensional examples and tests. |
| `VectorIndexKind::Faiss` | FAISS-backed HNSW over normalized vectors when GrapheneDB is compiled with `GRAPHENEDB_USE_FAISS=ON`. |

`inspect()` reports both:

```text
vector_index_requested=auto
vector_index=flat
```

or the resolved index selected for the open database.

The CLI exposes the same policy:

```bash
graphenedb_cli inspect /path/to/db 384 --vector-index flat --json
graphenedb_cli inspect /path/to/db 384 --vector-index faiss --json
graphenedb_cli search /path/to/db 384 0.1,0.2,... 131074 --vector-index kdtree
```

## Current Limitation

The current indexes are still rebuilt on open from durable node records. The optional FAISS backend adds an ANN implementation, but GrapheneDB does not yet persist the index structure itself, so enterprise GA still needs index-persistence work and target-hardware evidence.

For enterprise GA, GrapheneDB still needs vector-index persistence, recall/latency regression evidence on target hardware, and a final decision on whether the FAISS path is sufficient as the release backend.

## Recall Regression Gate

`graphenedb_vector_index_recall_bench` compares the configured vector index against exact flat search and reports mean recall@k, worst recall@k, and p95 latency for both paths.

Windows:

```powershell
.\scripts\run_vector_index_recall_bench.ps1 -Nodes 5000 -Queries 200 -Dim 32 -K 10 -Index auto -MinRecall 0.999
.\scripts\run_vector_index_recall_bench.ps1 -Nodes 5000 -Queries 200 -Dim 384 -K 10 -Index faiss -MinRecall 0.95
```

POSIX:

```bash
NODES=5000 QUERIES=200 DIM=32 K=10 INDEX=auto MIN_RECALL=0.999 scripts/run_vector_index_recall_bench.sh
NODES=5000 QUERIES=200 DIM=384 K=10 INDEX=faiss MIN_RECALL=0.95 scripts/run_vector_index_recall_bench.sh
```

Any future FAISS, HNSW, or DiskANN index must pass this recall gate and the larger storage/retrieval performance profile before it can be considered GA-ready.

The richer extraction-ingest and storage/retrieval benchmarks also accept explicit vector-index selection now, and the preview/enterprise evidence bundles should preserve both the requested and resolved index for those runs.
