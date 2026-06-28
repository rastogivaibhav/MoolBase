# v1 acceptance report

## Completed

| Gate | Status |
|---|---|
| Library structure | Pass |
| CLI | Pass |
| Framed WAL | Pass |
| Full node persistence | Pass |
| Full vector persistence | Pass |
| Full edge persistence | Pass |
| Transaction commit replay only | Pass |
| Uncommitted transaction ignored | Pass |
| Torn WAL tail handled | Pass |
| Fixed vector dimension | Pass |
| Invalid edge rejection | Pass |
| Snapshot delete visibility | Pass |
| Compaction | Pass |
| Backup | Pass |
| Lock file | Pass |
| Causal search bundle | Pass |
| ASAN/UBSAN | Pass |
| TSAN | Pass |
| Synthetic benchmark | Pass |

## Current benchmark sample

```text
nodes=15000 edges=10000
ingest_ms=5913.92
vector_p95_ms=1.85982 causal_p95_ms=0.271809 causal_hit_rate=1
```

## Honest classification

This is best described as:

> GrapheneDB v1 embedded causal-memory core — controlled pilot ready.

It is not yet:

> Public cloud database, Qdrant replacement, SQLite replacement, Neo4j replacement, or enterprise GA storage engine.
