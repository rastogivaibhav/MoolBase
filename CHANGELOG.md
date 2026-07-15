# Changelog

## 0.6.0-rc1 - Pilot release candidate

- Added graceful SIGTERM/SIGINT shutdown that stops admission, drains the bounded worker pool, checkpoints live state, and emits structured lifecycle events.
- Added public `/v1/version` capability discovery plus versioned response headers.
- Added strict HTTP framing checks for missing/conflicting `Content-Length`, unsupported transfer encodings, and non-JSON request bodies.
- Added durable `Idempotency-Key` support for node and temporal-fact writes.
- Changed server bulk insertion to one atomic `put_batch()` transaction and added a configurable bulk admission bound.
- Added a dependency-free Python pilot client, OpenAPI v1 contract, and pilot lifecycle/API contract test.
- Added bounded query parameters, temporal pagination, payload limits, client-error status mapping, and physical-capacity-safe idempotent replay.
- Made structured request and lifecycle logs atomic across worker and shutdown threads so every emitted line remains valid JSON.
- Bounded the ordinary CTest stress profile while retaining explicit 100k/1m scale gates, and added a single repeatable `run_pilot_rc1_gate.sh` workflow.
- Updated package installation to exclude Python bytecode/cache directories.

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

## Launch hardening iteration

- Replaced detached per-connection threads with a bounded fixed-size worker pool and overload rejection.
- Added per-client token-bucket rate limiting, request-size limits, socket timeouts, secure non-loopback bind checks, and constant-time API-key comparison.
- Added structured JSON request logs and Prometheus-compatible metrics.
- Added physical-lattice capacity preflight, readiness protection, and `/v1/admin/capacity`.
- Added non-root hardened container build, healthcheck utility, secure Compose example, and static Docker security validation.
- Added mixed server soak, adverse-filesystem, rate-limit, queue-overload, restart, and launch-hardening tests.
- Reworked semantic placement anchor selection to use vector/metadata candidate retrieval instead of scanning every node.
- Resolved strict compiler-warning findings in the DB and Kosh adapter paths.