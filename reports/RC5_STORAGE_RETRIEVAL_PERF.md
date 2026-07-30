# GrapheneDB RC5 Storage/Retrieval Performance

## Benchmark

`graphenedb_rc5_storage_retrieval_bench` measures:

- lattice-enabled incident/extraction ingest throughput
- vector search latency
- lattice-enabled causal search latency
- causal root-hit rate on realistic incident motifs
- metadata lookup cost on repeated service labels
- cross-layer, defect, and synthetic edge counts
- reopen/rebuild time
- validation after reopen

Default local command:

```bash
NODES=20000 QUERIES=100 DIM=64 scripts/run_rc5_storage_retrieval_bench.sh
```

Matrix smoke command:

```bash
scripts/run_rc5_perf_matrix.sh
```

The matrix script writes `reports/RC5_STORAGE_RETRIEVAL_MATRIX_OUTPUT.txt` and accepts custom cases:

```bash
CASES="100000:100:64 100000:100:384" scripts/run_rc5_perf_matrix.sh
```

## Required future matrix

- 100k, 1M nodes
- dimensions 64, 384, 768
- lattice retrieval enabled and disabled
- WAL fsync enabled and disabled
- compacted vs WAL-heavy reopen
- sparse vs dense causal/lattice edges
- low and high defect/cross-layer densities

## GA interpretation

This benchmark is an RC5 smoke/performance scaffold. It now uses richer incident-shaped extraction batches with explicit lattice coordinates, mixed bond types, cross-layer edges, and synthetic boundary bridges. It is still not a GA-scale performance certification until the matrix above is run on stable target hardware and results are preserved.
