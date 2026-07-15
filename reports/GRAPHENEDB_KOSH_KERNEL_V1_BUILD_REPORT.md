# GrapheneDB + Kosh Kernel v1 Build Report

## What was built
This iteration turns the merged architecture into a runnable prototype system:

- Physical hex-lattice DB server remains the storage engine.
- Temporal facts and provenance-rich edges remain first-class server APIs.
- Bundle retrieval remains the main reasoning retrieval primitive.
- Added semantic-causal-temporal lattice placement policy.
- Added lattice quality endpoint.
- Fixed JSON key parsing so values such as `"source":"incident"` do not get mistaken for the `incident` key.

## New / changed endpoints

### `POST /v1/facts`
Accepts `placement_policy: "semantic_causal_temporal"` plus fields such as `service`, `incident`, `account`, `repo`, `valid_from`, `valid_until`, and `fact_type`.

### `POST /v1/admin/lattice/quality`
Returns basic topology quality metrics:

- `local_coherence`
- `causal_locality`
- `temporal_consistency`
- `neighbor_pairs`

## Placement model
The new placement policy chooses candidate empty cells near existing anchors and scores them using:

```text
score(cell) = semantic affinity + metadata affinity + temporal affinity - overcrowding penalty
```

The mathematical claim is now explicit: hex distance is intended to approximate semantic-causal-temporal locality, rather than being only a spiral allocation scheme.

## Validation performed

Targeted build/tests:

- `graphenedb_server` build passed.
- `graphenedb_tests` passed.
- `graphenedb_physical_lattice_primary_tests` passed.
- `graphenedb_dense_hex_lattice_index_tests` passed.

Runtime demo:

- Inserted 6 temporal facts across two incident/service clusters.
- Inserted 5 provenance-rich edges.
- Verified empirical mode excludes the inferred shortcut.
- Verified balanced mode returns both compressed inferred shortcut and mechanistic path.
- Verified contradiction attachment in bundle output.
- Verified temporal retrieval for `as_of=2026-02-02`.
- Verified physical files exist after restart.
- Verified bundle retrieval still works after restart.

## Demo observation
For the incident example:

- Empirical mode returns `deployment -> leak -> outage`.
- Balanced mode returns both `deployment -> outage` inferred/compressed and `deployment -> leak -> outage` mechanistic.
- Contradictory status report is attached to the bundle.

## Current limitations

- The deterministic local embedding is still a preview implementation, not a production embedding model.
- Lattice quality metrics are basic; they prove instrumentation exists, not superiority.
- Placement is now meaning-aware, but still heuristic.
- Next proof should compare spiral vs semantic-causal-temporal placement on a 1k-10k benchmark corpus.

## Verdict
This is now a visible Kosh Kernel v1 prototype: physical memory storage + temporal/provenance reasoning retrieval + mathematically motivated placement instrumentation.
