# RC5 Lattice Review

## Review summary

RC5 moves the graphene-inspired lattice model from a durable annotation into a tested ingestion and retrieval path.

## Implemented in this pass

- Atomic `put_batch()` API with one WAL transaction for nodes and edges.
- CLI `import-tsv` path that applies semantic lattice placement and commits one batch.
- Deterministic lattice placement helper:
  - `hex_spiral_coord()`
  - `assign_lattice_batch()`
  - `PlacementStrategy::InputOrder`
  - `PlacementStrategy::SemanticGroups`
- ACID-oriented lattice test coverage:
  - v1 record compatibility
  - invalid batch rollback
  - snapshot visibility
  - compact/backup/reopen durability
  - cross-layer bond validation
- Storage/retrieval benchmark scaffold:
  - ingest throughput
  - vector search latency
  - lattice causal-search latency
  - reopen time

## Current architectural contract

GrapheneDB can now ingest memory as a batch of nodes and edges, where coordinates may be preassigned by an extractor or assigned deterministically by the built-in lattice placement helper. The DB validates lattice topology before the batch is committed, then writes all records inside one WAL transaction.

## Remaining review concerns

- `put_batch()` currently requires edge endpoints to use concrete node IDs. Callers inserting into a non-empty DB must set `LatticePlacementOptions::base_node_id` correctly.
- The placement helper now includes deterministic semantic grouping by incident/signature/role, but it is not yet an embedding-clustering, learned extractor, or space-filling-curve placement algorithm.
- No crash-injection hooks exist for I/O failure between batch WAL frames.
- Benchmarks are smoke-level until the larger RC5 matrix is run on stable target hardware.

## Recommendation

Treat RC5 as a hardening milestone, not GA. The lattice model is now core enough to test and benchmark, but extraction quality, crash injection, and larger-scale retrieval benchmarks remain the next gates.
