# Changelog

## v1 RC4 - Portability + Core Debt

- Added platform abstraction for POSIX/Windows-sensitive operations.
- Added Windows platform source and CMake guard for POSIX-only process-kill test.
- Removed hacky transactional PUT_NODE WAL frame construction.
- Persisted `next_txid` in MANIFEST and recover it on reopen.
- Added named `RetrievalTuning` constants in `DBOptions`.
- Replaced `__builtin_popcountll` with C++20 `std::popcount`.
- Added cached current-snapshot live node/edge counts.
- Added `VectorIndex` interface and `FlatVectorIndex` implementation.
- Added RC4 reviewer-response, platform support, and vector-index docs.


## v1.0-rc3 — Productization Pack

Added:

- Clean GitHub-ready repo hygiene.
- One-command scripts for build, tests, sanitizers, stress, crash matrix, fuzz smoke, examples, and packaging.
- GitHub Actions CI workflow.
- CMake presets.
- Graphene uniqueness demo comparing vector-only, graph-only, and causal-memory retrieval.
- API examples for coding memory, incident memory, and team brain.
- Better README.
- V1 RC acceptance report.
- GA readiness scorecard.
- Claude/engineer handoff receipt.
- Contributing and security docs.

## v1.0-rc2 — Extended Gates

Added:

- 1M-node storage stress.
- Process-kill crash matrix.
- Coverage-guided fuzz smoke target.
- WAL rotation/checkpointing.
- Metadata search API.
- Stale lock recovery.
- Kosh adapter TSV interchange gate.
- Long-soak test harness.

## v1.0-rc1 — Gate Pack

Added:

- Stress, crash, fuzz, and Kosh adapter tests.
- CLI smoke test.
- ASAN/UBSAN and TSAN gates.

## v0.17 — Original spike baseline

- Single-file Graphene/Sandhi retrieval experiment.
- Demonstrated signature-plane routing and causal path retrieval on synthetic data.
