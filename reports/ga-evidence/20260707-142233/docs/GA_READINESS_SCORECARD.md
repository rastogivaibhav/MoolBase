# GrapheneDB GA readiness scorecard

## Summary

| Area | Score | Status |
|---|---:|---|
| Product positioning | 8/10 | Clear as embedded causal/lattice memory DB; docs say where the graphene analogy stops. |
| Build/package hygiene | 9/10 | CMake install/export package, consumer smoke, release package scripts, CI gates. |
| Core API | 8/10 | Node/edge, batch, extraction ingestion, lattice, backup, validation, inspect APIs. |
| Durability | 8/10 | WAL/checkpoint/replay, WAL append rollback, manifest checksum/version validation, deterministic fault injection. |
| Safety | 8/10 | Validation, sanitizer gates, crash/fault matrix, corrupt-manifest coverage; longer fuzzing still required. |
| Performance | 7/10 | Storage/retrieval and extraction benchmarks with threshold checker; target-hardware large profiles still pending. |
| Operability | 8/10 | CLI import/extract/inspect/neighbors/validate/compact/backup, JSON automation output, package verification, and GA readiness harness. |
| Integration | 6/10 | TSV extraction API and Kosh adapter contract exist; live production Kosh/LLM-Kosh runtime pending. |
| Documentation | 9/10 | Architecture, lattice model, extraction, storage format, packaging, CI, GA verification, examples. |
| Enterprise GA | 6/10 | Strong controlled-pilot candidate, not yet external enterprise GA. |

## Current overall readiness

**Controlled pilot:** 8.5/10  
**Public developer preview:** 8/10  
**Enterprise GA:** 6/10

The detailed execution plan for closing the remaining ACID, storage/retrieval performance, extraction-boundary, packaging, and release-evidence gates is in [`NEXT_GA_EXECUTION_PLAN.md`](NEXT_GA_EXECUTION_PLAN.md).

## Must-fix before enterprise GA

1. Run a true 24-hour soak on a persistent machine.
2. Run multi-hour coverage-guided fuzzing and preserve corpus/crash artifacts.
3. Run the full default GA readiness harness on an approved build host without local policy exclusions.
4. Run approved-host 100k/1M-node rich-workload profiles at target dimensions (`100k >= 384`, `1M >= 768`) with realistic metadata, causal density, extraction records, and lattice bonds, and preserve the release evidence.
5. Run real disk-full, permission-denied, and rollback-failure tests on target filesystems.
6. Integrate against the real KoshDB/LLM-Kosh runtime if it remains part of the release story.
7. Complete the optional FAISS/HNSW vector index path with persistence, approved-host recall/latency evidence, and a final release-backend decision.
8. Add release signing, semantic version tags, and final license review.
9. Revisit encryption-at-rest and authentication/authorization if the product expands beyond the embedded-library security boundary.
10. Rehearse operational recovery procedures on target release hosts and preserve evidence.

## Already closed since the earlier RC scorecard

- The 100k and 1M stress gates now ingest richer extraction-style workloads with lattice coordinates, mixed bond types, metadata, and causal retrieval checks instead of flat node-only inserts.
- Batch ingestion API via `put_batch()`.
- Extraction ingestion API via `put_extraction()` and CLI `extract-tsv`.
- Versioned extraction schema v1 with source URI, extraction run ID, relation evidence fields, and unsupported-version rejection.
- Source-scoped external IDs and duplicate relation suppression.
- Lattice coordinates, bond/defect types, layer support, neighbor validation, and lattice-aware retrieval score.
- Manifest checksums, component format versions, and unsupported future format rejection.
- Deterministic WAL/checkpoint/manifest/backup failure injection.
- POSIX real permission-denied smoke for read-only WAL reopen and denied backup destination.
- POSIX disk-pressure smoke using `RLIMIT_FSIZE` to force WAL append failure and prove reopenable prior state.
- WAL byte-threshold rotation, CLI `compact`, and `inspect()` WAL/data byte counters.
- CLI `--json` output for inspect, validate, compact, and verified backup automation.
- Stale-lock recovery policy, strict lock mode, and operational recovery runbook.
- Configurable vector index policy with `Auto`, `Flat`, `KDTree`, and optional `Faiss` modes.
- Vector-index recall benchmark that compares the selected index against exact flat search.
- End-to-end operator flow scripts for extraction import, JSON monitoring, validation, compaction, backup, and retrieval.
- Recovery rehearsal script for verified backup restore, validation, retrieval, and lattice-neighbor checks.
- Runnable local Kosh adapter interchange gate with preserved output.
- Vector-only baseline comparison benchmark for causal retrieval behavior and latency.
- Runnable examples for contradiction, supersession, extraction import, and lattice propagation.
- Minimal C ABI for non-C++ callers and future Python bindings.
- Embedded-library security boundary documented in `SECURITY.md`.
- Release package SHA-256 sidecar and per-file manifest generation.
- CMake package install/export and external package consumer verification.
- GA readiness harness with preserved report directories and benchmark threshold checks.
- Windows `graphenedb_cli_extract_tests` now runs through a Python harness so local Device Guard policy does not block the full release-tree CTest suite.

## Should-fix before public developer preview

- Publish preserved benchmark reports for the intended developer-preview hardware profile.

## Deferred enterprise features

- Distributed clustering.
- SQL parser.
- Cloud SaaS control plane.
- Neo4j/Qdrant/SQLite replacement claims.
- Multi-tenant authentication server.
- Full repair/rebuild utility beyond validation, backup, compact, and reopen checks.

## Recommended release label

`GrapheneDB v0.5.0-rc5 - embedded causal/lattice-memory DB for controlled pilots`
