# GrapheneDB Storage Reduction and Enterprise Scale Specification

Status: evidence-backed design proposal, not an approved durable-format change

Date: 24 July 2026

Decision: approve with constraints

## A. Executive verdict

Reducing the settled core GrapheneDB datastore to less than 50% of the current
v2 checkpoint size is feasible without quantizing vectors or changing query
results.

Four real compacted fixtures were measured, covering 2,048 to 10,000 nodes,
2,643 to 12,915 edges, and vector dimensions from 2 to 384. An exact binary
candidate occupied 22.3% to 29.8% of the current persisted representation. A
zlib-compressed exact binary candidate occupied 3.5% to 19.8%.

The preferred first change is therefore an exact, versioned binary checkpoint
format. Lossy vector encodings are not justified for the primary empirical or
audit store. On the measured fixtures they saved relatively little beyond the
exact structural conversion at low dimensions and caused material ranking
changes. An optional lossy derived-search tier can be evaluated later, but it
must never become the authoritative representation.

The measured result applies to settled `MANIFEST + graphene.data +
graphene.wal`. The fixtures had an empty WAL. It does not yet prove a 50%
reduction for resident memory, physical-lattice sidecars, transient compaction
space, replicas, backups, or an unbounded WAL. Those layers require separate
budgets and acceptance evidence.

## B. Current storage anatomy

The v2 checkpoint writes tab-separated decimal fields, hexadecimal content and
metadata, and comma-separated decimal float vectors. Hex encoding alone
doubles string bytes. Repeated metadata strings are duplicated on nodes and
edges. Decimal float text is substantially larger than the authoritative
32-bit vector value.

Representative 4,096-node, 5,289-edge, 64-dimensional fixture:

| Component | Bytes | Percent | Exactness | Recommended treatment |
|---|---:|---:|---|---|
| Node and edge metadata as hex | 7,874,999 | 64.6% | Exact | Shared byte dictionary and dictionary IDs |
| Vector decimal text | 3,119,184 | 25.6% | Exact float32 | Little-endian IEEE-754 float32 blocks |
| Framing, fixed fields, delimiters | 805,436 | 6.6% | Exact | Binary headers, varints, delta coding |
| Content as hex | 226,160 | 1.9% | Exact | Length-prefixed raw bytes |
| Edge weights as decimal text | 84,624 | 0.7% | Exact to current persisted precision | Fixed-point integers matching v2 decimal semantics |
| Coordinates and edge endpoints | 74,429 | 0.6% | Exact | Zigzag/delta varints; derive only default lattice neighbours |
| Manifest and WAL | 297 | less than 0.1% | Exact | Versioned manifest; bounded WAL |
| **Total** | **12,185,129** | **100%** |  |  |

The dominant component changes with embedding dimension:

| Fixture | Persisted bytes | Metadata hex | Vector text | Persisted/logical-payload proxy |
|---|---:|---:|---:|---:|
| 10,000 nodes, dimension 2 | 22,467,754 | 85.9% | 1.0% | 2.23x |
| 4,096 nodes, dimension 8 | 9,449,232 | 83.3% | 4.1% | 2.25x |
| 4,096 nodes, dimension 64 | 12,185,129 | 64.6% | 25.6% | 2.38x |
| 2,048 nodes, dimension 384 | 13,753,218 | 28.5% | 67.2% | 2.66x |

The logical-payload proxy is decoded content, decoded metadata, float32
vectors, edge endpoints, and edge weights. It is not raw source-document size.
If "1 GB incoming" means source documents before chunking, embedding, edge
creation, or provenance enrichment, an additional measured enrichment factor
must be applied.

The result explains the observed expansion: 1 GB of post-enrichment logical
payload can become approximately 2.2 to 2.7 GB in the current compacted v2
format, before replicas, backups, WAL headroom, or temporary compaction space.

## C. Mathematical model

For a change that affects only quantizable weights:

```text
S1 / S0 = 1 - f + f*r + h
```

where `f` is the fraction occupied by weights, `r` is the new-to-old weight
rate, and `h` is encoding overhead.

For the dimension-64 fixture, vector text is only `f = 0.256` of total storage.
Even an ideal 50% reduction of that component gives:

```text
1 - 0.256 + 0.256*0.5 = 0.872
```

before quantizer metadata. Vector precision reduction alone cannot reach the
total 50% target on this fixture. At dimension 2, vectors are only 1.0% of
storage, so quantization is effectively irrelevant.

The exact binary candidate changes all structural terms:

```text
Ssettled = Sheader + Sdictionary + Snodes + Svectors + Sedges + Sindexes
Soperational = Ssettled + SWAL + Ssidecars + Stemp + Sreplicas + Sbackups
```

The application distortion objective is:

```text
D = 0.10 Dpoint
  + 0.10 Daggregate
  + 0.35 Drank
  + 0.20 Dthreshold
  + 0.20 Dpath
  + 0.05 Dtopology
```

The authoritative codec selects `D = 0`. Optional derived codecs must report
all terms rather than only mean squared error.

## D. Candidate comparison

The following table uses the dimension-64 real fixture. Timings are whole-file
Python harness timings and are comparative evidence, not C++ production
latency.

| Method | Total ratio | Exact? | Top-10 overlap | Threshold F1 | Application distortion | Encode/decode |
|---|---:|---|---:|---:|---:|---:|
| Current v2 text | 100.0% | Yes | 1.00 | 1.000 | 0 | Baseline |
| Exact binary float32 | 24.85% | Yes | 1.00 | 1.000 | 0 | 216/147 ms |
| zlib over exact binary | 9.77% | Yes | 1.00 | 1.000 | 0 | 99/11 ms |
| zlib over current v2 | 12.81% | Yes | 1.00 | 1.000 | 0 | 385/29 ms |
| Binary float16 | 20.55% | No | 0.88 | 1.000 | 0.04209 | 878/863 ms |
| Global int8 | 18.40% | No | 0.26 | 0.996 | 0.25984 | 979/349 ms |
| Block int8 | 18.40% | No | 0.28 | 0.997 | 0.25261 | 1,257/531 ms |
| Predictive residual int8 | 18.46% | No | 0.06 | 0.996 | 0.32980 | 1,570/1,851 ms |

Across all four real fixtures:

| Method | Best ratio | Worst ratio | Query semantics |
|---|---:|---:|---|
| Exact binary float32 | 22.3% | 29.8% | Preserved |
| zlib exact binary | 3.5% | 19.8% | Preserved after decode |
| zlib current v2 | 4.9% | 24.3% | Preserved after decode |
| Binary float16 | 18.3% | 23.4% | Ranking changed on measured and adversarial data |
| Global int8 | 12.6% | 23.4% | Material ranking and outlier sensitivity |

The synthetic adversarial scenario deliberately concentrated values around
ranking and threshold boundaries. Top-10 overlap fell to 0.54 even for
float16. On the low-dimensional real fixtures, float16 and int8 provided almost
no total-storage benefit beyond exact binary while changing rankings. Small
pointwise error is therefore not an adequate safety argument.

One residual per block can make a complete block sum exact:

```text
residual(B) = sum(original(B)) - sum(decoded(B))
```

It cannot make every strict subset of that block exact. Exact arbitrary range
sums require finer residual information approaching the information removed by
quantization. Block residuals are not part of the recommended primary codec.

## E. Pareto frontier

For the authoritative datastore, the frontier is:

1. exact binary float32 when direct access, simple updates, and low CPU cost
   dominate;
2. page-compressed exact binary when disk, backup, and network footprint
   dominate.

Lossy encodings are dominated for the current goal because exact binary already
beats the 50% target by a wide margin. The small additional reduction from
float16 or int8 does not justify ranking, threshold, and update risk.

`gzip`, `zlib`, `bzip2`, and `LZMA` were executed. `zstd`, LZ4, Snappy,
bitshuffle, and native columnar codecs were unavailable and remain explicit
benchmark gaps. Zstd is a plausible production page codec, but must not be
claimed superior until measured on the same fixtures and update workload.

## F. Recommended v3 architecture

The v3 checkpoint should be a page-oriented, endian-defined binary format
behind a core C++ codec interface. The embedded library remains authoritative.

### File header

```text
magic = "GDB3"
format_version
minimum_reader_version
feature_flags
dimension
checkpoint_txid
logical_version
page_size
page_count
directory_offset
header_checksum
```

All multibyte integers use little-endian encoding. Readers reject unknown
required features, invalid lengths, integer overflow, overlapping sections,
checksum failure, and unsupported versions before allocating from encoded
sizes.

### Page structure

```text
page_type
codec_id                 # none initially; optional zstd after evidence
uncompressed_length
compressed_length
record_count
first_id
last_id
payload_crc32c
compressed_payload
```

Use independently checksummed pages so point reads and corruption containment
do not require decompressing the full datastore.

### Dictionary section

- Store metadata keys and repeated values once as length-prefixed raw bytes.
- Reference dictionary entries with unsigned varints.
- Keep arbitrary user bytes exact.
- Allow page-local additions or a new dictionary generation so one new value
  does not rewrite the whole database.

### Node section

- Delta-varint node IDs and created versions.
- Varint deleted version, signature, and incident ID.
- Packed flags and defect type.
- Presence byte plus zigzag-varint `q`, `r`, and `layer`.
- Length-prefixed raw content.
- Dictionary ID pairs for metadata.

Coordinates remain exact. Geometric hex neighbours are computed from
coordinates. Only default geometric adjacency may be implicit; causal,
contradiction, supersession, defect, cross-layer, non-default-strength, and
provenance-bearing edges remain explicit.

### Vector section

- Exact IEEE-754 binary32 values in fixed-dimension node blocks.
- Block directory maps node ranges to vector pages.
- Optional page compression is independent of logical vector precision.
- A future approximate search index is a rebuildable derivative, never the
  authoritative vector representation.

### Edge section

- Delta-varint edge IDs and versions.
- Varint endpoint IDs.
- One-byte origin, role, bond, defect, and layer-coupling enums.
- Fixed-point confidence and bond strength matching current v2 persisted
  six-decimal semantics.
- Dictionary ID pairs for metadata.

"Exact" here means exact relative to values recoverable from the current v2
durable representation. The current decimal serializer already limits durable
double precision; migration cannot recover digits that v2 did not persist.

### Index section

The measured candidate includes:

- sparse node and edge page offsets;
- outgoing and incoming edge postings;
- metadata postings;
- coordinate postings.

Indexes are versioned and checksummed but rebuildable. A corrupt derived index
must never override canonical node or edge records.

### WAL and compaction

Phase 1 keeps WAL frame v1 unchanged and changes only checkpoint output. This
reduces migration and crash-recovery risk. To keep operational size bounded:

- configure byte-threshold WAL rotation;
- expose data, WAL, sidecar, and temporary-space metrics;
- reject compaction when free space is below a conservative bound;
- document that peak compaction space can exceed the settled 50% target.

Phase 2 may introduce a binary WAL only after v3 checkpoint recovery is proven.

## G. Query execution model

| Query | Compressed execution |
|---|---|
| Node by ID | Sparse page lookup, verify and decode one page |
| Metadata equality | Dictionary lookup plus postings; decode result pages |
| Lattice neighbours | Derive six coordinates, then coordinate index lookup |
| Causal traversal | Traverse exact adjacency postings; decode target pages |
| Vector search | Scan/decompress float32 blocks or use a rebuildable ANN index |
| Regional aggregate | Decode intersecting pages; exact because values are exact |
| Threshold/ranking | Exact float32 comparisons; deterministic tie policy required |
| Historical query | Use explicit time partitions; current snapshots are not durable retention points |

The base codec preserves point values, aggregates, rankings, thresholds,
topology, paths, updates, reproducibility, and auditability. Nothing requires
fallback access for normal reads. Original source artifacts should still live
in a content-addressed evidence store because GrapheneDB records are reasoning
objects, not a replacement for source-document retention.

## H. Migration plan

1. Add `CheckpointCodec` with v2 reader and v3 reader/writer implementations.
2. Add golden fixtures, property tests, fuzz tests, malformed-length tests,
   checksum tests, endian tests, and partial-page corruption tests.
3. Write a v3 checkpoint to a temporary sibling file while v2 remains
   canonical.
4. Reopen v3, run `validate()`, compare counts, IDs, vectors, metadata, edges,
   lattice coordinates, and deterministic query digests.
5. Shadow-read representative point, metadata, vector, lattice, causal, and
   temporal queries from both versions.
6. Flush the v3 file and parent directory, then atomically switch the manifest.
7. Retain the v2 checkpoint until the rollback window expires.
8. Roll back by restoring the v2 manifest and replaying only a version-compatible
   WAL. Do not claim rollback until mixed-version WAL tests pass.
9. Backfill shard by shard; never rewrite an entire enterprise fleet at once.

No durable-format constant should change until these tests and
`docs/STORAGE_FORMAT.md` are updated in the same review.

## I. Enterprise and hundreds-of-agents operating model

GrapheneDB is not a distributed database. PayPal-scale deployment must not turn
one embedded instance into a shared global bottleneck. Keep each embedded
database authoritative for its shard and put routing, tenancy, replication,
retention, and federation in an external platform layer.

```text
source/event streams
  -> durable ingestion log
  -> normalization, chunking, provenance and deduplication
  -> bounded batchers with idempotency keys
  -> tenant/domain/time shard router
  -> GrapheneDB shard set
  -> checkpoint/backup pipeline
  -> federated retrieval and dialectic controller
```

Recommended memory tiers:

| Tier | Contents | Retention |
|---|---|---|
| Agent working memory | Private scratch, proposed hypotheses, intermediate opposition traces | Hours to days; TTL |
| Shared evidence memory | Deduplicated observed/discovered facts and source references | Policy driven |
| Model-world memory | Typed hypotheses, abstractions, contradictions, decisions, outcomes | Versioned and governed |
| Source evidence store | Immutable content-addressed original artifacts | Compliance policy |
| Cold checkpoints | Compressed, encrypted shard checkpoints and audit exports | Recovery policy |

Do not copy the same document, embedding, or evidence object into every agent's
database. Store one governed shared evidence object and let agent-private
reasoning nodes reference it. Apply tenant, purpose, geography, and data-class
boundaries before deduplication; cross-boundary deduplication can itself leak
information.

The papers add a useful behavioral contract:

- TheHypoKosh supplies temporal facts, typed provenance, multiple causal paths,
  contradictions, no-evidence abstention, and the rule that reinforcement
  raises salience rather than truth confidence.
- The Dialectical Model World adds bounded expansion, convergence, opposition,
  re-expansion, synthesis, implementation, and outcome feedback.
- Convergence creates a view or abstraction node; it never deletes the
  underlying `FiberBundle` or minority evidence paths.
- Every loop has maximum rounds, wall-clock, token, read, and durable-write
  budgets.
- Hypothetical, inferred, reinforced, discovered, and observed objects retain
  distinct promotion policies.
- Implementation and outcome nodes close the feedback loop, but high-impact
  actions remain human-approved.

This separation controls both epistemic risk and storage growth. Most
intermediate reasoning traces remain short-lived. Only evidence-backed or
governance-approved products are promoted to durable shared memory.

### Capacity example

Let:

```text
A = active agents
I = logical post-enrichment GB per agent per day
e = enrichment factor when I is measured before embeddings/edges
alpha = current persisted/logical expansion
r = new/current storage ratio
T = retention days
F = replicas, backups, and headroom multiplier

capacity = A * I * e * alpha * r * T * F
```

For 100 agents and 1 GB per agent per day of post-enrichment logical data:

| Scenario | Daily primary | 90-day primary | With 30% headroom and two additional copies |
|---|---:|---:|---:|
| Current v2, measured range | 223-266 GB | 20.1-23.9 TB | 78.3-93.4 TB |
| Exact binary v3, measured range | 49.8-79.2 GB | 4.5-7.1 TB | 17.5-27.8 TB |

If 1 GB is raw source data, multiply both rows by measured `e`; do not assume
`e = 1`.

The fixtures contain roughly 1.0 to 2.5 KB of logical payload per node. A
100-GB/day logical stream therefore corresponds to approximately 460 to 1,150
node writes per second before bursts and dialectic amplification. The existing
12,000-node evidence reported 867 nodes/second on one embedded profile. A
single instance is consequently too close to saturation for the worst measured
case. Begin production experiments with at least eight independently
recoverable shards, cap each shard well below benchmark saturation, and size
again from p95/p99 ingest, checkpoint, recovery, and query measurements. Eight
is a starting experiment, not a proven production count.

Required platform controls:

- per-tenant and per-agent byte, node, edge, query, and dialectic-loop quotas;
- bounded queues and admission backpressure;
- idempotent bulk ingestion;
- content-addressed source and embedding deduplication;
- time/domain partitioning and federated top-k merge;
- hot/warm/cold retention with legal holds;
- encryption, key separation, audit export, and deletion workflows;
- shard health, WAL growth, checkpoint duration, compaction space, RSS, and
  query-tail metrics;
- failure isolation so one dialectic loop cannot exhaust shared capacity.

## J. Validation evidence and final decision

Reproduce a report with:

```powershell
python scripts/storage_reduction_benchmark.py `
  --db <compacted-db-directory> `
  --output reports/storage-reduction/<name>.json
```

Executed evidence:

- four real compacted stores;
- 2,048 to 10,000 nodes;
- 2,643 to 12,915 edges;
- dimensions 2, 8, 64, and 384;
- exact binary, float16, int16, int8, int4, block int8, predictive residual
  int8, gzip, zlib, bzip2, and LZMA candidates;
- uniform, Gaussian, sparse, heavy-tailed, spatially smooth, spatially
  discontinuous, outlier-heavy, temporal-drift, and adversarial synthetic
  vector scenarios;
- decode, checksum, byte-size, point error, regional aggregate, top-10,
  threshold, path, topology, and application-distortion checks;
- GrapheneDB Release suite: 27/27 tests passed before this study resumed.

Decision: **approve with constraints**.

Approve an exact v3 checkpoint prototype and migration test suite. Do not
approve lossy primary storage. Do not claim production-wide 50% total cost
reduction until the following are measured:

1. a representative enterprise corpus and a real 1-GB ingestion sample;
2. WAL-inclusive steady-state and burst storage;
3. physical-lattice sidecar size;
4. resident memory and allocator overhead;
5. zstd/LZ4 page-codec throughput;
6. update amplification and random-read latency in the C++ implementation;
7. compaction peak space and crash recovery;
8. replicas, backups, encryption, and object-store costs;
9. multi-shard federated recall and tail latency;
10. 24-hour and 72-hour soak on intended production hardware.

If exact v3 fails any invariant or does not remain below 50% on representative
production data, retain v2 and classify the result as insufficient evidence.
The target must not be recovered by silently weakening audit, provenance,
ranking, or path semantics.
