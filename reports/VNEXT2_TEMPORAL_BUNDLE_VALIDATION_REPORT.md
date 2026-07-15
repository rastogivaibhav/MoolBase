# GrapheneDB vNext2 — Temporal Bundle Validation Report

## Scope
This iteration hardens the minimal HypoKosh-M layer without adding the deferred research loop. It keeps the product as a physical hex-lattice AI memory server and improves the retrieval layer.

## Changes added
- `POST /v1/retrieve/bundle` now accepts `as_of` / `at` and filters path traversal by temporal validity.
- Added `POST /v1/retrieve/temporal` for as-of temporal fact lookup.
- Bundle retrieval now attaches contradiction evidence found on any node in the returned path bundle, not only contradictions directly on the final target.
- Bundle JSON now reports `as_of`, `temporal_consistent`, `attached_contradictions`, path roles, origins, confidence, and degeneracy.

## Validation run
Built with Release CMake target set and tested server runtime with API-key auth and physical lattice primary enabled.

### Build/tests passed
- `graphenedb_tests_passed=true`
- `physical_lattice_primary_tests_passed=true`
- `dense_hex_lattice_index_tests_passed=true`

### Runtime scenario
Stored five temporal facts:
1. old February fact: Patch X considered safe; valid until 2026-03-01
2. March fact: Patch X introduced memory leak
3. Memory leak caused heap saturation
4. Heap saturation caused service outage
5. Contradictory status report denying memory pressure

Stored four provenance-rich edges:
- observed/mechanistic: A -> B
- discovered/causal: B -> C
- inferred/compressed: A -> C, derived from A -> B and B -> C
- observed/contradicts: D -> B

### Behaviour proven
- Empirical mode returned only the mechanistic/discovered path to C and excluded the inferred compressed shortcut.
- Balanced mode returned both:
  - compressed inferred shortcut A -> C
  - mechanistic path A -> B -> C
- Balanced mode attached the contradiction edge D -> B to the bundle for C because B participates in the path to C.
- Temporal lookup at `2026-02-15` returned only the February safe fact.
- Temporal lookup at `2026-03-03` returned the four March-valid facts and excluded the expired February fact.
- Provenance validation returned `ok=true`.
- Restart persistence passed: node content, bundle paths, degeneracy, and attached contradiction survived restart.

## Runtime latency from local smoke
- Bundle retrieval p50: ~1.15 ms
- Bundle retrieval p95: ~1.35 ms

## Files confirmed
The runtime DB created physical persistence files:
- `graphene.lattice.bin`
- `graphene.nodeidx`
- `graphene.wal`
- `graphene.lattice`

## Honest status
This is stronger than the previous vNext because temporal validity and contradiction attachment now affect retrieval output. It is still not a production GA server. The main remaining work is a quality benchmark against vector-only and graph-only baselines, plus larger server-level bundle retrieval benchmarking.

## Next recommended proof
Build a benchmark corpus with temporal supersession, contradictions, compressed shortcuts, and incomplete graph edges. Compare:
- vector-only retrieval
- graph-only traversal
- GrapheneDB lattice + provenance bundle retrieval

The key metric should be answer/context quality, not only latency.
