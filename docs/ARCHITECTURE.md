# Architecture

GrapheneDB is organised into four layers.

## 1. Storage layer

- `graphene.wal`: append-only framed transaction log.
- `graphene.data`: compacted durable state.
- `MANIFEST`: dimension and version metadata.
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

## 2. Data model

The primitive is:

```text
memory node + causal edge + versioned evidence path
```

Nodes contain content, vector, signature, metadata, incident/group id, and visibility versions.
Edges contain from/to endpoints, origin, role, confidence, metadata, and visibility versions.

## 3. Retrieval layer

GrapheneDB combines:

- vector similarity
- signature-plane candidate reduction
- reverse causal graph traversal
- empirical filtering of hypothetical/analogical edges
- explainable memory bundle output

## 4. Operations layer

- `compact()` writes live visible records to `graphene.data` and truncates WAL.
- `backup()` copies data, WAL, and manifest.
- `validate()` checks vector dimensions and edge endpoint integrity.
