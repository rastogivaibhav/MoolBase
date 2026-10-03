# Architecture

GrapheneDB is organised into four layers.

## 1. Storage layer

- `graphene.wal`: append-only framed transaction log.
- `graphene.data`: compacted durable state.
- `MANIFEST`: dimension, logical version, txid, and storage-format metadata.
- `LOCK`: single writable process guard.

WAL records are framed as:

```text
payload_length|fnv1a_checksum|payload\n
```

Transactions use:

```text
BEGIN <txid>
PUT_NODE <txid> ...
PUT_EDGE <txid> ...
DELETE_NODE <txid> ...
COMMIT <txid>
```

Replay rule: only records inside committed transactions are applied. Pending/uncommitted transactions are ignored.

Current manifest/storage/WAL/lattice/extraction format versions are documented in [`STORAGE_FORMAT.md`](STORAGE_FORMAT.md) and surfaced by `inspect()`.

## 2. Data model

The primitive is:

```text
memory node + causal edge + versioned evidence path
```

Nodes contain content, vector, signature, metadata, incident/group id, and visibility versions.
Edges contain from/to endpoints, origin, role, confidence, metadata, and visibility versions.

Joint-causality hyperedges are a checked logical view over an atomically
committed set of ordinary durable edges. Their shared metadata declares an
`all_sources` requirement; this adds no new durable record type or format
version.

Nodes may also contain durable graphene-inspired lattice coordinates (`q`, `r`, `layer`) and defect annotations. Edges may contain durable bond type, defect type, layer coupling, and bond strength. In lattice-required mode, nodes without coordinates are rejected and lattice bonds are validated against same-layer hex-neighbor or explicit cross-layer rules.

## 3. Retrieval layer

GrapheneDB combines:

- vector similarity
- signature-plane candidate reduction
- optional lattice propagation through validated graphene-inspired bonds
- reverse causal graph traversal
- empirical filtering of hypothetical/analogical edges
- explainable memory bundle output
- optional bounded dialectic expansion/convergence/opposition over immutable
  bundles, with typed temporal/provenance validation and all-source traversal
- optional read-only HypoKosh proposal generation over the dialectic result;
  every generated proposal remains hypothetical and carries discriminating
  tests rather than becoming durable truth

### Governed outcome-learning boundary

`OutcomeLearningEngine` records immutable outcome episodes and approved policy
decisions as ordinary nodes through the authoritative `put_extraction()` core
transaction. This deliberately introduces no durable record type or format
version. WAL, checkpoint, backup, replay, second-open rejection, and lattice
rules therefore remain the database correctness boundary.

The evaluator reads verified training and development episodes to compare exact
retrieval-policy configurations. Evaluation-split and legal-hold episodes
cannot influence fitting or selection. Safety-negative episodes remain visible
and veto a candidate. Evaluation produces a recommendation with no durable
writes; activation and rollback are separate approved, append-only events.

This is policy learning, not LLM-weight training. Tenant IDs namespace records
inside one pilot database but do not provide cryptographic tenant isolation.

## 4. Operations layer

- `compact()` writes live visible records to `graphene.data` and truncates WAL.
- Historical snapshot versions are in-process isolation views, not durable
  retention points. Compaction preserves only the current visible state.
- `DBOptions::wal_rotate_bytes` checkpoints and truncates WAL once the byte threshold is reached.
- `backup()` copies data, WAL, and manifest; restore is verified by opening the copied directory and running `validate()`.
- `validate()` checks vector dimensions and edge endpoint integrity.
- `inspect()` reports `wal_rotate_bytes`, `wal_bytes`, and `data_bytes` so operators can verify retention behavior.
- `graphenedb_cli validate`, `graphenedb_cli compact`, and `graphenedb_cli backup ... --verify` expose those checks operationally.
- `graphenedb_cli` accepts `--wal-rotate-bytes N` as a common option for import, extraction, inspect, validation, backup, and compact workflows.
- `graphenedb_cli` accepts `--vector-index auto|flat|kdtree|faiss` to choose the vector index for the current open. `faiss` is available only when GrapheneDB is compiled with `GRAPHENEDB_USE_FAISS=ON`.
- Stale `LOCK` files whose owner process is no longer alive are recovered on open by default; `--no-recover-stale-lock` makes CLI opens fail instead for strict operator checks.
- `graphenedb_cli --json` emits machine-readable JSON for inspect, validate, compact, and backup automation.

### Controlled-pilot server boundary

The optional HTTP server remains an adapter over the embedded library. In
particular, `POST /v1/extractions` parses and bounds a versioned JSON request,
generates the pilot server's deterministic vectors, and calls
`GrapheneDB::put_extraction()` once. It does not resolve relations or issue
per-node writes itself. This preserves core validation, one-transaction
atomicity, WAL replay, and durable source-scoped idempotency.

The endpoint limits nodes by the configured `--max-bulk-nodes`, limits
relations to 50,000, limits metadata and text amplification, rejects unknown
or reserved fields, and uses the same bounded worker queue, authentication,
request-size limit, bind policy, and reverse-proxy contract as other pilot
routes.

The learning routes follow the same adapter rule. Episode and decision writes
call `OutcomeLearningEngine`, which calls `put_extraction()` once. HypoKosh and
policy evaluation are read-only. The server generates the pilot vector and
lattice coordinate, but it does not duplicate episode identity, utility,
eligibility, promotion, rollback, or legal-hold logic.

`BatchInput::delete_node_ids` retires existing visible nodes atomically with batch insertions. Invalid deletion references, duplicate deletions, or edges to retired endpoints reject the whole batch before writing. The embedded API preserves existing WAL records and replay rules.
