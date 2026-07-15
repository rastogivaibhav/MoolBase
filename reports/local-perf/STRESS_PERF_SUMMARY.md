# GrapheneDB Local Stress and Performance Summary

Scope: read `AGENTS.md` / `CODEX.md`, built Release binaries with tests and benchmarks enabled, then ran correctness, stress, retrieval and vector-index benchmarks in this sandbox environment.

Repository guidance observed:
- Treat RC5 as public developer-preview, not enterprise GA.
- Do not add new features during PR stabilization.
- Enterprise-GA proof still requires approved-host readiness, 24h soak, fuzzing, target-scale profiles, real filesystem failure evidence, signing/license review and vector-backend decisions.

## Build

- Configure: `cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=OFF`
- Build: `cmake --build build-perf -j 2`
- Result: build passed.

## Correctness smoke

| Test | Result | Runtime | Max RSS |
|---|---:|---:|---:|
| Core API tests | pass | 0.03s | 18 MB |
| Lattice tests | pass | 0.01s | 8 MB |
| Physical lattice sidecar tests | pass | 0.01s | 6 MB |
| Extraction ingest tests | pass | 0.01s | 7 MB |

## Stress and performance runs

| Run | Nodes | Edges | Index | Ingest | Vector p95 | Causal p95 | Reopen | Max RSS | Result |
|---|---:|---:|---|---:|---:|---:|---:|---:|---|
| RC stress scaled | 12,000 | 15,999 | auto -> flat | 680.6 nodes/s | 1.091 ms | 0.229 ms | 488 ms | 141 MB | pass |
| RC stress scaled, forced KDTree | 12,000 | 15,999 | kdtree | 966.5 nodes/s | 0.027 ms | 0.321 ms | 558 ms | 145 MB | pass |
| 1M storage gate scaled | 6,000 | 7,750 | auto -> flat | 2.333s total ingest | 1.593 ms | 0.185 ms | 219 ms | 75 MB | pass |
| Storage/retrieval bench | 2,000 | 2,664 | auto -> flat | 3,687 nodes/s | 0.465 ms | 0.124 ms | 93 ms | 29 MB | pass |
| Storage/retrieval bench | 5,000 | 6,664 | auto -> flat | 2,305 nodes/s | 0.515 ms | 0.223 ms | 205 ms | 63 MB | pass |
| Storage/retrieval bench | 10,000 | 13,331 | auto -> flat | 1,707 nodes/s | 0.875 ms | 0.292 ms | 316 ms | 116 MB | pass |
| Vector baseline comparison | 15,000 | 5,000 | n/a | 0.699s ingest | 1.210 ms | 0.418 ms | n/a | 24 MB | pass |
| Vector recall benchmark | 5,000 | n/a | auto -> flat | n/a | 0.492 ms candidate p95 | n/a | n/a | 17 MB | pass, recall@10 = 1.0 |

## Failed / incomplete long runs

- `ctest --output-on-failure` did not complete because the default `graphenedb_rc_stress_tests` target exceeded the execution window.
- `graphenedb_rc_1m_storage_tests --nodes 24000 ...` did not complete within the execution window.
- `graphenedb_rc_stress_tests --incidents 10000` / ~60k nodes did not complete within the execution window.

These are not correctness failures, but they are performance/readiness concerns for this environment.

## Findings

1. Correctness gates for core, logical lattice, physical lattice sidecar and extraction ingestion pass.
2. The new physical lattice storage preview has a working test target, but it is not stress-tested at high scale separately from the normal DB path.
3. Auto vector selection resolves to `flat` for 64-dimensional embeddings by design. That makes recall exact, but large runs become slow.
4. For 12k nodes, forcing `kdtree` improved vector p95 from ~1.09 ms to ~0.027 ms and ingest from ~681 nodes/s to ~967 nodes/s, with similar memory usage.
5. Reopen time grows visibly with node count because current indexes are rebuilt on open, which the docs already call out as a GA limitation.
6. The DB is suitable for developer-preview and controlled-pilot experiments, but this run does not prove enterprise GA.

## Recommended next engineering work

1. Add a dedicated `physical_lattice_stress_tests` target that writes `physical_lattice_storage=true` at 100k+ cells and validates neighbour-slot integrity after reopen.
2. Add persisted vector-index snapshots or a rebuild cache to reduce reopen cost.
3. Change benchmark scripts to include explicit `--vector-index kdtree` and `--vector-index flat` comparisons for dimensions where KDTree is valid.
4. Add timeout-aware progress logging to long stress tests so CI and agents can distinguish hang versus slow progress.
5. Run the official preview hardware profile and enterprise GA campaign on an approved host, not this sandbox.

Raw output: `reports/local-perf/raw_output.txt`.
