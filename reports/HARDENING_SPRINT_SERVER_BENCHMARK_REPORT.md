# GrapheneDB Hardening Sprint — Server Benchmark & Validation Report

## Scope
This sprint hardened the physical-hex DB server and then validated behaviour through API, persistence, lattice retrieval, hybrid search, physical file inspection, and dense-lattice stress tests.

## Code changes made

1. **Thread-per-connection server handling**
   - The server now handles each accepted socket in a detached worker thread instead of serially processing all clients on the accept loop.

2. **Concurrent physical lattice placement fix**
   - Found during benchmark: concurrent writes could collide on the same spiral coordinate because placement used `db.node_count()` outside the DB write lock.
   - Fixed by adding an atomic server-side lattice slot allocator initialized from the current node count.

3. **Operational endpoints added**
   - `GET /v1/metrics`
   - `POST /v1/admin/validate`
   - `POST /v1/edges`
   - `GET /v1/search/lattice?node_id=<id>&hops=<n>`

4. **Benchmark evidence added**
   - `reports/hardening/server_benchmark_600nodes.json`
   - `reports/hardening/dense_index_unit_output.txt`
   - `reports/hardening/dense_lattice_stress_24571_output.txt`

## Build/test validation

Passed targeted tests:

```text
graphenedb_tests_passed=true
graphenedb_lattice_tests_passed=true
physical_lattice_primary_tests_passed=true
dense_hex_lattice_index_tests_passed=true
```

## Server benchmark result

Environment: local container, Release build, `graphenedb_server`, dimension 64, physical lattice primary enabled.

Workload:

- 500 sequential node inserts
- 199 edge inserts
- 100 concurrent node inserts with 32 client workers
- point reads
- 50 lattice neighbourhood queries, hops=2
- 20 hybrid vector searches
- validation call
- metrics call
- restart + persistence check

Results:

```json
{
  "sequential_inserts": {
    "count": 500,
    "writes_per_second": 183.005,
    "p50_ms": 5.126,
    "p95_ms": 10.109,
    "max_ms": 24.605
  },
  "edge_inserts": {
    "count": 199,
    "p50_ms": 8.665,
    "p95_ms": 15.854
  },
  "concurrent_inserts": {
    "count": 100,
    "workers": 32,
    "writes_per_second": 111.373,
    "p50_ms": 139.821,
    "p95_ms": 157.446,
    "max_ms": 158.957
  },
  "point_reads": {
    "count": 7,
    "p50_ms": 0.668,
    "p95_ms": 0.921
  },
  "lattice_search_hops2": {
    "queries": 50,
    "avg_neighbors": 17.68,
    "p50_ms": 0.592,
    "p95_ms": 0.710,
    "max_ms": 0.750
  },
  "hybrid_search": {
    "queries": 20,
    "p50_ms": 0.786,
    "p95_ms": 4.545,
    "max_ms": 4.710
  },
  "validate": {
    "status": 200,
    "ok": true,
    "report_excerpt": "OK"
  },
  "restart_persistence": {
    "node42_status": 200,
    "node42_has_content": true,
    "lattice_status": 200,
    "lattice_neighbors_after_restart": 18,
    "node_count_after_restart": 600
  }
}
```

## Dense physical hex-lattice benchmark

Unit-level dense index:

```text
dense_hex_lattice_index_tests_passed=true
dense_hex_radius=20
dense_hex_nodes=1261
dense_hex_insert_ms=1478.31
dense_hex_neighbor_probes=1000
dense_hex_neighbor_query_ms=1.04537
dense_hex_neighbor_avg_us=1.04537
dense_hex_total_neighbors=17050
```

Stress-level dense lattice:

```text
dense_hex_lattice_stress_passed=true
dense_hex_stress_radius=90
dense_hex_stress_nodes=24571
dense_hex_stress_batch_insert_ms=168.63
dense_hex_stress_neighbor_probes=5000
dense_hex_stress_neighbor_query_ms=10.8921
dense_hex_stress_neighbor_avg_us=2.17841
dense_hex_stress_total_neighbors=88857
```

## Physical files confirmed

After the server benchmark, the DB directory contained:

```text
graphene.wal
graphene.lattice
graphene.lattice.bin
graphene.nodeidx
MANIFEST
LOCK
```

This confirms that the server is not only storing logical nodes; it is maintaining the physical fixed-offset lattice index and node-to-lattice mapping.

## Behavioural assessment

### What is now credible

- It is a **working DB server prototype**, not only an embedded library.
- It supports write/read/search over HTTP.
- It persists data across restart.
- It creates and uses physical lattice files.
- Lattice neighbourhood retrieval is consistently sub-millisecond at small server scale.
- Dense-lattice local retrieval remains microsecond-class in embedded tests.
- Concurrent insert coordinate collision has been found and fixed.

### What is not yet Silicon-Valley-grade

- HTTP server is still custom/minimal, not production HTTP infrastructure.
- No authentication/TLS.
- No binary protocol/gRPC.
- No production-grade embedding provider adapter yet.
- Single-process, single-node only.
- Concurrent writes are correct after the placement fix, but throughput is not high yet because the write path still serializes through DB durability and lattice rewrite behaviour.
- Server-level 100k/1M-node benchmark has not been proven in this environment.

## Investment verdict

This is worth **one more serious productization sprint** because the core idea now has evidence:

- physical hex-lattice storage exists,
- dense neighbourhood retrieval is fast,
- the server works,
- persistence works,
- validation works,
- and a real concurrency bug was discovered and fixed.

But it should be positioned as an **AI memory database/server**, not a general database competing with Postgres/Qdrant/Neo4j.

The next proof needed for external credibility:

1. Replace custom HTTP with a real HTTP/gRPC stack.
2. Add reproducible benchmark harness with fixed datasets and automatic charts.
3. Run 100k, 1M, and 10M server-level profiles.
4. Add production embedding adapters: OpenAI-compatible, Ollama, Vertex AI.
5. Add auth, backup, restore, compaction, metrics, and Docker compose.
6. Publish a benchmark paper comparing:
   - GrapheneDB lattice retrieval
   - Qdrant vector-only retrieval
   - SQLite/FTS baseline
   - Neo4j explicit-edge traversal

## Bottom line

GrapheneDB is not yet a mature database company product, but it is no longer just a clever experiment. It is becoming a credible **agent-memory storage engine** with a differentiated physical locality model.
