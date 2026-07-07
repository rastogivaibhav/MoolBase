# Extraction Ingest Performance Gate

`graphenedb_extraction_ingest_bench` measures the first-class extraction path:

- document-style `ExtractionInput` batches
- source-scoped external IDs
- explicit multi-layer lattice coordinates
- lattice bond validation
- sigma/pi/van-der-Waals/defect/synthetic relation mix
- document metadata lookup cost
- idempotent re-import
- vector retrieval latency
- causal/lattice retrieval latency and expected-root hit rate
- close/reopen validation time

Run:

```bash
DOCS=100 NODES_PER_DOC=50 QUERIES=100 DIM=64 scripts/run_extraction_ingest_bench.sh
```

The benchmark emits:

- `extract_ingest_total_ms`
- `extract_nodes_per_sec`
- `extract_doc_p50_ms`
- `extract_doc_p95_ms`
- `idempotent_doc_p50_ms`
- `idempotent_doc_p95_ms`
- `vector_p50_ms`
- `vector_p95_ms`
- `causal_lattice_p50_ms`
- `causal_lattice_p95_ms`
- `metadata_doc_p50_ms`
- `metadata_doc_p95_ms`
- `causal_root_hit_rate`
- `avg_lattice_neighbors`
- `avg_lattice_score`
- `reopen_ms`

## GA Interpretation

This is an RC/GA-readiness gate for the ingestion path users are expected to call from extraction services. It now exercises explicit lattice geometry, layered propagation, bond/defect variety, metadata indexing, de-duplication, retrieval, reopen, and validation under a repeatable rich synthetic workload.

It is not yet a GA performance certification. GA still requires preserved results from stable target hardware, larger document counts, production embedding dimensions, cold-cache and warm-cache runs, and regression thresholds in CI.
