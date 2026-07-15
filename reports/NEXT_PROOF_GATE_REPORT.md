# GrapheneDB Next Proof Gate: 100k Server Benchmark + Crash Recovery

## Executive verdict

GrapheneDB is now a **credible prototype AI-memory DB server** with a differentiated physical hex-lattice storage/index path. The next iteration fixed the biggest scalability bottleneck found in the previous server: every node insert used to rebuild and rewrite the entire physical lattice sidecar. The server now updates the primary physical lattice binary incrementally and rebuilds it from WAL on open/recovery.

This is still **not enterprise GA**. It is now strong enough to justify one serious productization sprint aimed at public developer preview quality.

## Code changes made in this iteration

- Added incremental dense-lattice node insertion instead of full dense index rebuild on every write.
- Added incremental fixed-offset `graphene.lattice.bin` cell updates.
- Added append-only `graphene.nodeidx` updates for node-to-cell mapping.
- Updated open/recovery path to rebuild the primary physical lattice binary from WAL/data state.
- Updated validation so primary physical lattice mode validates `graphene.lattice.bin` and checks cell checksum/node match.
- Added `/v1/nodes/bulk` server endpoint for server-level bulk write benchmarking.

## Test results

| Gate | Result |
|---|---:|
| Core test | Passed |
| Physical lattice primary test | Passed |
| Dense hex lattice index test | Passed |
| 10k concurrent HTTP insert benchmark | Passed |
| 100k server bulk ingest | Passed |
| 100k restart persistence | Passed |
| 100k validation | Passed |
| Crash during active writes | Passed |

## 10k HTTP benchmark

| Metric | Result |
|---|---:|
| Nodes inserted | 10000 |
| Failures | 0 |
| Total time | 6.37 sec |
| Throughput | 1571 writes/sec |
| Insert p50 | 8.69 ms |
| Insert p95 | 20.50 ms |
| Insert p99 | 28.97 ms |
| Point read p50 | 0.48 ms |
| Lattice search p50 | 0.61 ms |
| Hybrid search p50 | 1.33 ms |
| Validation | OK |

## 100k server bulk benchmark

| Metric | Result |
|---|---:|
| Nodes stored | 100,000 |
| Ingest time | 24.06 sec |
| Bulk ingest throughput | 4157 nodes/sec |
| Point read p50 | 0.63 ms |
| Lattice search p50 | 0.62 ms |
| Hybrid search p50 | 9.30 ms |
| Validation time | 806.06 ms |
| Validation result | OK |
| Restart node count | 100,000 |

### 100k physical files

| File | Size |
|---|---:|
| `graphene.wal` | 89.13 MB |
| `graphene.lattice.bin` | 10.74 MB |
| `graphene.nodeidx` | 2.10 MB |
| `MANIFEST` | 287 bytes |

## Crash recovery during active writes

| Metric | Result |
|---|---:|
| ACKed writes before kill | 5,166 |
| Max ACKed node id | 5,165 |
| Node count after restart | 5,166 |
| Validation after restart | OK |
| Validation time | 41.51 ms |
| Lattice after restart | HTTP 200, 0.59 ms |

## Behaviour assessment

### What is now proven

1. The server is not just a library wrapper; it can accept writes over HTTP, persist data, restart, and serve reads/searches.
2. The physical hex lattice is not only metadata. `graphene.lattice.bin` is a fixed-offset physical index with cell checksums.
3. Dense lattice retrieval remains sub-millisecond at 100k nodes in this environment.
4. WAL replay plus physical lattice rebuild works after a hard process kill.
5. The previous insert scalability flaw was real and has been materially improved.

### What remains weak

1. HTTP implementation is still a hand-written minimal server, not production-grade HTTP/2/gRPC.
2. Bulk endpoint is benchmark-oriented; a real batch API should accept arrays/NDJSON and report partial failures properly.
3. WAL is large because checkpoint/compaction policy is not production tuned.
4. Vector search is still a simple in-process KD-tree/flat style path; for high-dimensional real embeddings, an HNSW/FAISS-style backend is needed.
5. No authentication model beyond API key.
6. No replication, clustering, tenant isolation, rate limiting, or cloud-native operator yet.

## Silicon Valley positioning

Do **not** pitch this as a generic database. Pitch it as:

> A physical hex-lattice memory server for AI agents, combining durable memory storage, dense neighbourhood retrieval, vector search, and causal/evidence graph semantics.

The impressive proof point is not "we made another DB." The proof point is:

> At 100k memories, local lattice recall stays sub-millisecond, survives restart, and recovers from a crash while writes are active.

## Investment recommendation

Invest **one more serious productization sprint**.

Recommended next gates:

1. Replace hand-rolled HTTP with gRPC + a real HTTP gateway.
2. Add NDJSON/array batch ingest with partial-failure semantics.
3. Add HNSW/FAISS backend for high-dimensional embeddings.
4. Add snapshot checkpointing so WAL does not grow unchecked.
5. Add 1M-node benchmark on a proper dev machine.
6. Build a demo use case: incident memory, coding memory, or Salesforce customer-meeting memory.

If the 1M-node benchmark and HNSW integration hold, this becomes worth showcasing publicly as **Graphene Memory Server**, not just GrapheneDB.
