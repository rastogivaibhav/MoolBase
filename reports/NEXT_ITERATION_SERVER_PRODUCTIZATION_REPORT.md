# GrapheneDB Next Iteration — Server Productization & Validation

## Scope implemented

This iteration moved the DB server closer to product-grade validation rather than only raw prototype behaviour.

Implemented changes:

- Optional API-key protection for all endpoints except `/v1/health`.
  - `--api-key <key>`
  - accepts `x-api-key: <key>` or `Authorization: Bearer <key>`.
- Admin endpoints added:
  - `POST /v1/admin/backup`
  - `POST /v1/admin/compact`
  - `GET /v1/admin/inspect`
  - existing `POST /v1/admin/validate`
- Backup logic extended to include physical lattice artefacts:
  - `graphene.lattice.bin`
  - `graphene.nodeidx`
  - `graphene.lattice`
  - WAL/manifest files
- Server benchmark harness added in evidence package.
- Revalidated physical-lattice primary mode after code changes.

## Build and test evidence

Targeted build completed for:

- `graphenedb`
- `graphenedb_server`
- `graphenedb_tests`
- `graphenedb_lattice_tests`
- `graphenedb_physical_lattice_primary_tests`
- `graphenedb_dense_hex_lattice_stress_tests`

Passed tests:

```text
graphenedb_tests_passed=true
graphenedb_lattice_tests_passed=true
physical_lattice_primary_tests_passed=true
dense_hex_lattice_stress_passed=true
```

Dense embedded lattice stress:

```text
dense_hex_stress_radius=40
dense_hex_stress_nodes=4921
dense_hex_stress_neighbor_probes=5000
dense_hex_stress_neighbor_avg_us=0.986219
```

## Server benchmark results

Environment: local loopback HTTP server in this execution container.

Dataset inserted through HTTP API:

- 300 sequential inserts
- 300 concurrent inserts
- 600 total nodes
- physical lattice primary enabled
- API key enabled

### Latency summary

| Operation | Count | p50 | p95 | Notes |
|---|---:|---:|---:|---|
| Sequential insert | 300 | 3.64 ms | 8.01 ms | includes HTTP + embedding preview + WAL/storage |
| Concurrent insert | 300 | 57.58 ms | 71.13 ms | 8 client workers; no failed inserts |
| Point read | 100 | 0.55 ms | 0.70 ms | HTTP read path |
| Lattice search | 100 | 0.57 ms | 0.66 ms | 2-hop physical hex neighbourhood |
| Hybrid search | 50 | 0.73 ms | 0.90 ms | deterministic embedding + vector search |

### API correctness checks

Passed:

- health endpoint
- API-key rejection for protected endpoint
- node insertion
- concurrent node insertion
- point read
- lattice search
- hybrid search
- validate endpoint
- backup endpoint
- compact endpoint
- restart persistence
- post-restart read
- post-restart lattice search
- post-restart validation

API-key protection proof:

```json
{"error":"unauthorized"}
```

Post-restart read proof:

```json
{
  "id": 42,
  "content": "customer memory 42: refund escalation renewal risk signal 8"
}
```

Post-restart lattice proof:

```json
{
  "neighbors": [9,10,21,22,23,24,40,41,43,44,65,66,67,68,69,98,99,100]
}
```

## Physical files observed

Server DB directory after benchmark:

```text
LOCK
MANIFEST
graphene.data
graphene.lattice
graphene.lattice.bin
graphene.nodeidx
graphene.wal
```

Backup directory after admin backup:

```text
MANIFEST
graphene.lattice
graphene.lattice.bin
graphene.nodeidx
graphene.wal
```

Note: in this benchmark the backup was taken before explicit compaction. The WAL therefore contained the active data state. A production backup contract should make this clearer by either forcing a checkpoint before backup or writing a backup manifest explaining whether data is represented by checkpoint files, WAL, or both.

## Current behaviour verdict

The server now behaves like a credible prototype DB server:

- It accepts network writes.
- It persists data across restart.
- It performs protected admin operations.
- It retrieves by physical hex-lattice neighbourhood.
- It retrieves by hybrid/vector search.
- It validates its state.
- It can produce a backup copy of the physical lattice index files.

It is still not production GA.

## Why it is worth continuing

The differentiated behaviour is real: once memories are physically placed into a dense hex lattice, a node can retrieve local neighbourhood context without requiring explicit graph edges. That gives GrapheneDB a distinctive AI-memory retrieval story rather than competing as a generic vector DB.

The benchmark numbers are good enough for prototype credibility:

- sub-millisecond point reads
- sub-millisecond 2-hop lattice search
- sub-millisecond hybrid search on the current dataset
- microsecond-level embedded dense-lattice retrieval

## What must be fixed before a Silicon Valley-grade public claim

Next gates:

1. Replace hand-rolled HTTP with a real HTTP/gRPC layer.
2. Add a backup manifest and checkpoint-before-backup mode.
3. Run 100k+ node server-level benchmark, not only embedded dense stress.
4. Add crash/restart server test during active writes.
5. Add write throughput benchmark with stable p50/p95/p99.
6. Add persistent vector-index snapshots.
7. Add a real embedding adapter behind the deterministic preview embedder.
8. Add SDK-level tests in Python/TypeScript.
9. Add a reproducible benchmark script under `bench/` or `tools/`.
10. Define the public product claim as: AI memory DB/server with physical hex-lattice neighbourhood retrieval.

## Investment recommendation

Invest one more serious sprint.

Do not yet spend months on a full database company path. Spend the next sprint proving a strong public benchmark:

- 100k nodes via server
- 1M embedded nodes
- p50/p95/p99 for writes, reads, lattice search, hybrid search
- crash-recovery evidence
- benchmark vs SQLite+vector baseline or Qdrant-lite baseline

If those numbers hold, GrapheneDB becomes credible as a differentiated memory engine.
