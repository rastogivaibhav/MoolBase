# Changelog

## Unreleased - Complete discrete Lyapunov critic

- Replaced heuristic-only runtime use of `StabilityCriticV0` with `LyapunovCritic`.
- Added a seven-dimensional bounded epistemic error state and mode-aware goal set.
- Added a positive weighted quadratic energy function with reported lower and upper
  quadratic bounds.
- Added per-cycle drift, sufficient-decrease, contraction, practical-stability,
  convergence, divergence, oscillation, and finite-state limit-cycle analysis.
- Added deterministic Lyapunov trajectory receipts to CLI, HTTP, and model-world
  metadata while retaining `StabilityCriticV0` as a compatibility facade.
- Added direct mathematical/adversarial tests and extended complete runtime and live
  server contracts. Full release regression passes 45/45 tests.

## Unreleased - Complete TheHypoKosh experimental runtime

- Added an immutable, deterministic `FiberBundle` with exact duplicate removal,
  source-lineage-aware independent-path counting, saturating degeneracy, and
  stable bundle hashes.
- Added `StabilityCriticV0` for temporal consistency, path diversity, independent
  support, provenance quality, contradiction, pattern lock, and missing evidence.
  The implementation is Lyapunov-inspired but makes no formal control-theory claim.
- Added bounded `CorrectiveEscape`, convergence/opposition/re-expansion orchestration,
  governed answer statuses, and complete per-layer reasoning receipts.
- Added recursive self-healing plans that may propose labelled repairs and discovery
  questions but cannot silently promote inferred or hypothetical claims.
- Added a typed, checksummed model-world ledger, persistence/reload, audit scheduler,
  relation ontology, ambiguity-preserving entity resolution, and generic structured
  relation ingestion.
- Added the `graphenedb_cli reason` command and authenticated
  `POST /v1/reason/runtime` server endpoint.
- Added unit, integration, adversarial, CLI, persistence, generic-domain, and live
  server contract tests for the complete runtime.
- Kept the separate Windows stale-lock shared-read candidate outside this runtime;
  it still requires review and clean Windows validation before being claimed.

## Unreleased - P0 correctness remediation

- Kept C++ assertions active in Release test targets so contract tests cannot
  silently become no-ops.
- Made the optional POSIX HTTP server opt-in on Windows while preserving normal
  embedded-library and CLI builds.
- Replaced the single-writer lock race with exclusive file creation.
- Hardened WAL recovery: complete corrupt frames now fail open, while only an
  unterminated final fragment is repaired and durably truncated.
- Made checkpoint and derived-file replacement durable and atomic, including
  bounded Windows sharing-violation retries.
- Prevented failed pre-commit writes from consuming transaction IDs, node IDs,
  or snapshot versions.
- Separated committed write success from post-commit maintenance failures and
  exposed maintenance state through inspection.
- Enforced physical-lattice radius and layer bounds before committing a node.
- Enforced `create_if_missing=false` without creating the requested database
  directory.
- Corrected vector-index defaults and causal-confidence ranking semantics.
- Brought the OpenAPI contract into alignment with the implemented
  controlled-pilot routes and limits.
- Added focused P0, OpenAPI-surface, WAL, locking, and Windows lifecycle
  regressions.
- Added an experimental read-only dialectic reasoning layer with bounded
  multi-root path expansion, immutable convergence, deterministic opposition,
  bounded re-expansion, provenance findings, structured synthesis, Kosh
  adapter access, and a dedicated contract test.
- Added strict RFC3339 temporal views, reusable provenance assessment, atomic
  metadata-compatible all-source hyperedges, authenticated bounded dialectic
  HTTP/OpenAPI support, and a clearly labelled synthetic ablation benchmark.
- Added a repeatable Linux Docker validation image and a source-traceable
  real-postmortem comparison benchmark covering vector-only, existing causal,
  and dialectic multi-root retrieval.
- Exposed the authoritative extraction transaction as bounded authenticated
  `POST /v1/extractions`, added conflict-safe durable replay semantics,
  OpenAPI/Python-client support, rollback and restart contracts, and an
  API-level real-postmortem evaluation.
- Added experimental `GDB-GL-0` governed outcome learning: read-only HypoKosh
  proposals, immutable verified episodes, deterministic data-use classes,
  train/development policy comparison with evaluation-split exclusion,
  safety vetoes, explicit approved promotion/rollback history, legal-hold
  aware quarantine, authenticated HTTP/OpenAPI/Python support, and Docker
  restart validation. This does not train LLM weights or autonomously promote
  truth.

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
