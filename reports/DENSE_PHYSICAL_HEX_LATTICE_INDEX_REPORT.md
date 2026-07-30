# Dense Physical Hexagonal Lattice Index Report

## Summary

Implemented a dense physical hex-lattice retrieval path for `DBOptions::physical_lattice_storage=true`.

This moves the preview beyond a sidecar-only physical layout. The DB now maintains an in-memory dense projection of the physical `graphene.lattice` sidecar:

- `dense_lattice_cells`: deterministic layer/ring/q/r ordered physical cells.
- `dense_lattice_ordinal_by_node`: O(1) node-to-cell lookup.
- Each dense cell has six fixed neighbour slots matching the axial hex directions.
- `lattice_neighbors()` uses the dense six-slot physical topology when enabled.
- `causal_search()` lattice propagation now uses a HexWave-style dense neighbour expansion when enabled.
- Explicit lattice bonds still contribute strength/filters where present.
- Physical adjacency can retrieve nearby hex cells even when every adjacency is not stored as a separate edge record.

## Retrieval Algorithm: HexWave Dense Expansion

For each anchor:

1. Resolve node id to dense cell ordinal in O(1).
2. Expand through the six physical neighbour slots.
3. Apply lattice decay per hop.
4. Use explicit bond strength where an edge exists.
5. Fall back to physical-adjacency strength for direct hex neighbours when no edge exists.
6. Preserve empirical filtering for hypothetical/analogical explicit bonds.

This gives fast locality retrieval over a dense hexagonal lattice without scanning generic edge vectors.

## Validation Run

Environment: local container, Release build.

Commands run:

```bash
cmake -S . -B build-hex -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON
cmake --build build-hex -j2 --target graphenedb_dense_hex_lattice_index_tests graphenedb_dense_hex_lattice_stress_tests
build-hex/graphenedb_physical_lattice_storage_tests
build-hex/graphenedb_lattice_tests
build-hex/graphenedb_tests
build-hex/graphenedb_dense_hex_lattice_index_tests 20
build-hex/graphenedb_dense_hex_lattice_stress_tests 182 10000
```

Observed results:

```text
physical_lattice_storage_tests_passed=true
graphenedb_lattice_tests_passed=true
graphenedb_tests_passed=true
dense_hex_lattice_index_tests_passed=true
dense_hex_radius=20
dense_hex_nodes=1261
dense_hex_insert_ms=1228.61
dense_hex_neighbor_probes=1000
dense_hex_neighbor_query_ms=0.956106
dense_hex_neighbor_avg_us=0.956106

dense_hex_lattice_stress_passed=true
dense_hex_stress_radius=182
dense_hex_stress_nodes=99919
dense_hex_stress_batch_insert_ms=707.654
dense_hex_stress_neighbor_probes=10000
dense_hex_stress_neighbor_query_ms=28.9927
dense_hex_stress_neighbor_avg_us=2.89927
```

## Interpretation

The dense lattice retrieval path is now fast under a near-100k physical hex-lattice test: approximately 2.9 microseconds per two-hop neighbour probe in this environment.

Important limitation: the dense sidecar is still derived from the canonical WAL/data store. The DB is now physical-lattice-indexed for retrieval, but WAL/data remain the recovery source of truth.

## Next Hardening Steps

- Persist/reload dense index metadata directly from `graphene.lattice` on open.
- Add incremental sidecar updates instead of full materialisation for every write path.
- Add 24-hour soak with radius 182+ and mixed insert/delete/compact/reopen cycles.
- Add crash/failure matrix specifically for `graphene.lattice.tmp` rename/failure behaviour.
- Benchmark HexWave causal search against edge-vector propagation on realistic mixed-bond corpora.
