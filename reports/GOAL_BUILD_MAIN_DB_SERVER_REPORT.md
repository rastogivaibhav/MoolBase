# GrapheneDB Goal Build: Main DB Server + Physical Hex Index

## Scope implemented

This build extends the dense physical hex-lattice preview toward a main database server shape.

Implemented in this pass:

1. **Physical fixed-offset lattice index**
   - Added `DBOptions::physical_lattice_primary`.
   - Added `DBOptions::physical_lattice_radius`.
   - Writes `graphene.lattice.bin` as a binary fixed-offset axial-disk lattice index.
   - Writes `graphene.nodeidx` as a persisted `node_id -> lattice ordinal` map.
   - Keeps framed WAL as the commit/recovery log.
   - Keeps `graphene.data`/WAL as compatibility truth for existing recovery.

2. **Mathematical storage model**
   - Uses axial hex coordinate `(q, r, layer)`.
   - Cube coordinate derived as `s = -q - r`.
   - Disk membership: `max(abs(q), abs(r), abs(s)) <= R`.
   - Capacity per layer: `1 + 3R(R + 1)`.
   - Fixed record offset:
     `offset = sizeof(Header) + (layer * cells_per_layer + axial_disk_ordinal(q,r,R)) * sizeof(PhysicalCellRecord)`.

3. **Cell record**

```cpp
struct PhysicalCellRecord {
  int32_t q;
  int32_t r;
  int32_t layer;
  uint32_t node_id;
  uint32_t neighbor_nodes[6];
  uint64_t vector_dim;
  uint64_t content_bytes;
  uint32_t flags;
  uint32_t checksum;
};
```

4. **Server process**
   - Added `graphenedb_server` executable.
   - Minimal HTTP API on `127.0.0.1`.
   - Endpoints:
     - `GET /v1/health`
     - `POST /v1/nodes`
     - `GET /v1/nodes/{id}`
     - `GET /v1/search/lattice?node_id=N`
     - `POST /v1/search/hybrid`
   - Server can generate deterministic local embeddings from text for preview/testing.
   - Server auto-places nodes in spiral hex coordinates.

5. **CLI**
   - Added `--physical-lattice-primary` option.

6. **Tests**
   - Added `tests/test_physical_lattice_primary.cpp`.
   - Validates binary lattice file, node index file, reopen, and lattice-neighbour retrieval.

## Validation evidence

Commands run successfully in this environment:

```text
physical_lattice_primary_tests_passed=true
graphenedb_tests_passed=true
graphenedb_lattice_tests_passed=true
physical_lattice_storage_tests_passed=true
dense_hex_lattice_index_tests_passed=true
dense_hex_lattice_stress_passed=true
```

Dense hex stress sample:

```text
dense_hex_stress_radius=30
dense_hex_stress_nodes=2791
dense_hex_stress_batch_insert_ms=23.2407
dense_hex_stress_neighbor_probes=500
dense_hex_stress_neighbor_query_ms=0.69089
dense_hex_stress_neighbor_avg_us=1.38178
```

Server smoke test:

```text
GET /v1/health
=> {"status":"ok","engine":"physical-hex-lattice"}

POST /v1/nodes
=> {"id":0,"q":0,"r":0,"layer":0}

POST /v1/nodes
=> {"id":1,"q":1,"r":0,"layer":0}

GET /v1/search/lattice?node_id=0
=> {"neighbors":[1]}
```

Generated physical files from server smoke:

```text
graphene.lattice      framed sidecar
graphene.lattice.bin  fixed-offset physical lattice index
graphene.nodeidx      node_id to lattice ordinal map
graphene.wal          commit log
```

Note: the file is named `graphene.lattice.bin` in the implementation.

## What remains partial

This is **not yet a full enterprise DB server**. It is a working prototype of the major phases.

Still needed for production-grade server:

1. mmap read path directly from `graphene.lattice.bin` instead of rebuilding in-memory dense index on open.
2. WAL replay that mutates binary lattice pages directly as the canonical storage path.
3. Separate `graphene.vec`, `graphene.edge`, and `graphene.blob` append files.
4. Real JSON parser and proper HTTP framework/gRPC.
5. Auth, TLS/proxy model, structured logs, OpenTelemetry metrics.
6. BYO embedding and provider adapters: OpenAI-compatible, Vertex, Bedrock, Ollama, ONNX.
7. Snapshot isolation across server requests and better concurrent write controls.
8. Large-scale soak: 100k/1M/10M cells and crash/failure matrix on approved hardware.

## Honest status

This build proves the architecture can move from “lattice as metadata/sidecar” to “fixed-offset physical hex index plus server access.” The canonical store is still compatibility WAL/data plus physical binary index. The next hard step is to make `graphene.lattice.bin` the direct mmap-backed read path and eventually the primary page store for commits.
