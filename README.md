# GrapheneDB v0.5.0 RC5

**GrapheneDB** is an experimental embedded **causal/lattice-memory database** for AI agents, coding assistants, incident-memory systems, research-pack ingestion, and team-brain workflows.

It is not trying to replace SQLite, Qdrant, or Neo4j. Its niche is narrower:

> Store AI memory as versioned nodes, embeddings, metadata, and causal/contradiction/supersession edges, then retrieve an explainable evidence bundle rather than only a list of similar chunks.

## Status

**Current maturity:** v0.5.0 RC5 / controlled pilot + public-developer-preview candidate.

RC5 builds on the earlier productization and portability work with durable graphene-inspired lattice fields, extraction ingestion, batch writes, GA readiness scripts, installable CMake packaging, release manifests, configurable vector-index policy, and richer operator/retrieval examples.

It is not yet external enterprise GA. Remaining GA work is documented in [`docs/GA_READINESS_SCORECARD.md`](docs/GA_READINESS_SCORECARD.md), with the execution plan in [`docs/NEXT_GA_EXECUTION_PLAN.md`](docs/NEXT_GA_EXECUTION_PLAN.md).

This branch also adds the first durable graphene-inspired lattice model: hexagonal node coordinates, lattice bond/defect metadata, topology validation, and optional lattice-aware retrieval propagation. The model is a memory topology and retrieval primitive, not a carbon-physics or material-science simulator.

It also adds a first-class extraction ingestion API for source-scoped external IDs, idempotent re-import, relation resolution, automatic lattice placement, and CLI ingestion. See [`docs/EXTRACTION_INGESTION.md`](docs/EXTRACTION_INGESTION.md).


## Platform support

| Platform | Status | Notes |
|---|---|---|
| Linux | CI target | Release build/test, package smoke, fuzz/sanitizer smoke, and GA readiness smoke are configured in CI; preserve CI artifacts for release evidence. |
| macOS | CI target | POSIX platform layer should apply; CI matrix includes macOS build/test. |
| Windows | Local smoke validated with policy caveat | Release build, focused CTest gates, CLI/operator flows, package verification, and release-manifest smoke have run locally. `graphenedb_cli_extract_tests` now runs through a Python harness on Windows so the release-tree CTest suite can complete locally. Some other newly linked test executables can still be blocked by local Windows Application Control and must be rerun on an approved release host. |

See [`docs/PLATFORM_SUPPORT.md`](docs/PLATFORM_SUPPORT.md).

## What makes it different?

| System | Primary primitive | Main answer |
|---|---|---|
| SQLite | table row | What exact data did I store? |
| Qdrant-style vector DB | vector point + payload | What is semantically similar? |
| Graph DB | node + relationship | How is data connected? |
| GrapheneDB | memory node + vector + causal edge + versioned evidence path | What is relevant, connected, causal, current, and explainable? |

GrapheneDB's target result is a `MemoryBundle`:

```text
semantic candidates
+ signature-plane reduction
+ causal path to root memory
+ optional lattice propagation through validated hex-neighbor bonds
+ contradiction/supersession signals
+ snapshot version
+ confidence/reason codes
```

## Core capabilities in this RC

- C++20 embedded library
- CLI tool
- Fixed-dimension vector validation
- Versioned memory nodes and edges
- Edge roles: causal, contradicts, supports, supersedes, predictive, analogical, etc.
- Query modes: empirical, balanced, theoretical
- Vector search
- Causal memory bundle retrieval
- Durable hexagonal lattice coordinates, bond/defect metadata, and optional lattice-aware retrieval
- Extraction ingestion API with stable external IDs and idempotent re-import
- Metadata search API
- Framed checksummed WAL
- Committed-transaction replay
- Torn-tail WAL handling
- WAL rotation/checkpointing
- Compaction
- Backup
- Stale lock recovery
- Process-kill crash tests
- Sanitizer gates
- Coverage-guided fuzz smoke target
- KoshDB/LLM-Kosh adapter skeleton and TSV interchange gate

## Quick start

```bash
./scripts/build_release.sh
./scripts/run_all_tests.sh
./scripts/run_graphene_uniqueness_demo.sh
```

Manual CMake:

```bash
cmake --preset release
cmake --build --preset release -j2
ctest --preset release --output-on-failure
./build-release/graphenedb_uniqueness_demo
```

## CLI smoke example

```bash
./build-release/graphenedb_cli init /tmp/gdb-demo 3
./build-release/graphenedb_cli put-node /tmp/gdb-demo 3 "root cause" 0.9,0.1,0.0 131074 root
./build-release/graphenedb_cli put-node /tmp/gdb-demo 3 "checkout timeout" 0.1,0.9,0.0 131074 symptom
./build-release/graphenedb_cli put-edge /tmp/gdb-demo 3 0 1 causal
./build-release/graphenedb_cli search /tmp/gdb-demo 3 0.1,0.9,0.0 131074
./build-release/graphenedb_cli validate /tmp/gdb-demo 3
./build-release/graphenedb_cli backup /tmp/gdb-demo 3 /tmp/gdb-demo-backup
```

Automation-friendly commands accept `--json`, for example:

```bash
./build-release/graphenedb_cli inspect /tmp/gdb-demo 3 --json
./build-release/graphenedb_cli validate /tmp/gdb-demo 3 --json
./build-release/graphenedb_cli inspect /tmp/gdb-demo 3 --vector-index flat --json
./build-release/graphenedb_cli inspect /tmp/gdb-demo 384 --vector-index faiss --json
```

Extraction TSV import:

```bash
./build-release/graphenedb_cli extract-tsv /tmp/gdb-demo 3 research-pack ./extraction.tsv
```

## Developer orientation

GrapheneDB is a compact embedded C++ database, not a service. The core code lives in `include/graphene/` and `src/`, with a CLI in `tools/graphenedb_cli.cpp`, C ABI glue in `include/graphene/c_api.h` and `src/c_api.cpp`, benchmarks in `bench/`, and release/operator automation in `scripts/`.

The fastest way to understand the repository is:

1. Read this README for product scope and maturity.
2. Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for storage, WAL, snapshots, retrieval, and CLI structure.
3. Read [`docs/GRAPHENE_LATTICE_MODEL.md`](docs/GRAPHENE_LATTICE_MODEL.md), [`docs/LATTICE_RETRIEVAL.md`](docs/LATTICE_RETRIEVAL.md), and [`docs/EXTRACTION_INGESTION.md`](docs/EXTRACTION_INGESTION.md) for the RC5 additions.
4. Read [`docs/NEXT_GA_EXECUTION_PLAN.md`](docs/NEXT_GA_EXECUTION_PLAN.md) and [`reports/GA_STATUS_REPORT.md`](reports/GA_STATUS_REPORT.md) before changing release claims.

Developer rules of thumb:

- Preserve the embedded-library boundary: do not add a network server, auth layer, SQL layer, or distributed cluster behavior unless the release plan explicitly changes.
- Keep durable format changes intentional and documented in [`docs/STORAGE_FORMAT.md`](docs/STORAGE_FORMAT.md), with fixtures or migration tests where applicable.
- Treat `put_batch()` and `put_extraction()` as public API surface; add tests before changing semantics.
- Prefer deterministic tests and preserved report output for release gates.
- Generated build directories are ignored. Preserved release evidence belongs under `reports/` only when it is intentionally part of a release candidate.

Agent/developer orientation files are provided at the repository root:

- [`CLAUDE.md`](CLAUDE.md) for Claude-style coding agents.
- [`AGENTS.md`](AGENTS.md) for Codex-style coding agents.

Handoff next step: open a PR from `codex/rc5-developer-preview` into `master`, present it as the RC5 public developer-preview candidate, link the preserved evidence in `reports/GA_STATUS_REPORT.md`, and fix only CI or reviewer issues needed to merge. Enterprise-GA work remains a separate follow-up track.

## One-command validation scripts

| Script | Purpose |
|---|---|
| `scripts/build_release.sh` | Configure and build release artifacts. |
| `scripts/run_all_tests.sh` | Build and run full CTest suite. |
| `scripts/run_sanitizers.sh` | Run selected ASAN/UBSAN and TSAN gates. |
| `scripts/run_100k_stress.sh` | Run 100k-class stress profile. |
| `scripts/run_1m_stress.sh` | Run 1M-node storage stress profile. |
| `scripts/run_crash_matrix.sh` | Run process-kill and recovery tests. |
| `scripts/run_extraction_ingest_bench.sh` | Benchmark extraction ingestion, idempotent re-import, and retrieval. |
| `scripts/run_vector_baseline_bench.ps1` | Compare vector-only top-1 retrieval against Graphene causal retrieval on Windows. |
| `scripts/run_vector_baseline_bench.sh` | Compare vector-only top-1 retrieval against Graphene causal retrieval on POSIX. |
| `scripts/run_vector_index_recall_bench.ps1` | Compare configured vector-index recall/latency against exact flat search on Windows. |
| `scripts/run_vector_index_recall_bench.sh` | Compare configured vector-index recall/latency against exact flat search on POSIX. |
| `scripts/run_extraction_ingest_bench.ps1` | Benchmark extraction ingestion, idempotent re-import, and retrieval on Windows. |
| `scripts/run_rc5_storage_retrieval_bench.ps1` | Benchmark storage, retrieval, reopen, and lattice search on Windows. |
| `scripts/verify_package_install.ps1` | Build, install, and consume the installed CMake package on Windows. |
| `scripts/verify_package_install.sh` | Build, install, and consume the installed CMake package on POSIX. |
| `scripts/run_ga_readiness.ps1` | Run the Windows GA readiness bundle and preserve logs. |
| `scripts/run_ga_readiness.sh` | Run the POSIX GA readiness bundle and preserve logs. |
| `scripts/run_preview_hardware_profile.ps1` | Preserve a reviewable developer-preview benchmark bundle on Windows. |
| `scripts/run_preview_hardware_profile.sh` | Preserve a reviewable developer-preview benchmark bundle on POSIX. |
| `scripts/package_release_install.ps1` | Build, verify, and archive the Windows install package. |
| `scripts/package_release_install.sh` | Build, verify, and archive the POSIX install package. |
| `scripts/collect_ga_evidence.ps1` | Collect docs, reports, GA logs, package hashes, and an evidence manifest on Windows. |
| `scripts/collect_ga_evidence.sh` | Collect docs, reports, GA logs, package hashes, and an evidence manifest on POSIX. |
| `scripts/write_ga_status_report.ps1` | Generate a current GA status report from local evidence and the scorecard on Windows. |
| `scripts/write_ga_status_report.sh` | Generate a current GA status report from local evidence and the scorecard on POSIX. |
| `scripts/run_release_candidate_bundle.ps1` | Run GA smoke, package the install tree, and archive one reviewable evidence bundle on Windows. |
| `scripts/run_release_candidate_bundle.sh` | Run GA smoke, package the install tree, and archive one reviewable evidence bundle on POSIX. |
| `scripts/run_fuzz_smoke.sh` | Build and run LLVM libFuzzer smoke target. |
| `scripts/run_examples.sh` | Run all API examples and demo programs. |
| `scripts/run_graphene_uniqueness_demo.sh` | Run the vector vs graph vs Graphene demo. |
| `scripts/run_operator_flow.ps1` | Run the Windows end-to-end operator flow with extraction, JSON monitoring, compaction, backup, and retrieval. |
| `scripts/run_operator_flow.sh` | Run the POSIX end-to-end operator flow with extraction, JSON monitoring, compaction, backup, and retrieval. |
| `scripts/run_recovery_rehearsal.ps1` | Rehearse backup restore, validation, retrieval, and neighbor checks on Windows. |
| `scripts/run_recovery_rehearsal.sh` | Rehearse backup restore, validation, retrieval, and neighbor checks on POSIX. |
| `scripts/run_kosh_adapter_gate.ps1` | Run the local Kosh adapter interchange gate on Windows. |
| `scripts/run_kosh_adapter_gate.sh` | Run the local Kosh adapter interchange gate on POSIX. |

## Key docs

- [`docs/GRAPHENE_UNIQUENESS.md`](docs/GRAPHENE_UNIQUENESS.md)
- [`docs/GRAPHENE_LATTICE_MODEL.md`](docs/GRAPHENE_LATTICE_MODEL.md)
- [`docs/LATTICE_RETRIEVAL.md`](docs/LATTICE_RETRIEVAL.md)
- [`docs/EXTRACTION_INGESTION.md`](docs/EXTRACTION_INGESTION.md)
- [`docs/PACKAGING_DISTRIBUTION.md`](docs/PACKAGING_DISTRIBUTION.md)
- [`docs/C_API.md`](docs/C_API.md)
- [`docs/OPERATIONAL_RECOVERY.md`](docs/OPERATIONAL_RECOVERY.md)
- [`reports/RECOVERY_REHEARSAL.md`](reports/RECOVERY_REHEARSAL.md)
- [`docs/GA_READINESS_VERIFICATION.md`](docs/GA_READINESS_VERIFICATION.md)
- [`docs/CI_RELEASE_AUTOMATION.md`](docs/CI_RELEASE_AUTOMATION.md)
- [`reports/RELEASE_CANDIDATE_BUNDLE.md`](reports/RELEASE_CANDIDATE_BUNDLE.md)
- [`reports/GA_STATUS_REPORT.md`](reports/GA_STATUS_REPORT.md)
- [`docs/STORAGE_FORMAT.md`](docs/STORAGE_FORMAT.md)
- [`docs/V1_RC_ACCEPTANCE_REPORT.md`](docs/V1_RC_ACCEPTANCE_REPORT.md)
- [`docs/GA_READINESS_SCORECARD.md`](docs/GA_READINESS_SCORECARD.md)
- [`docs/NEXT_GA_EXECUTION_PLAN.md`](docs/NEXT_GA_EXECUTION_PLAN.md)
- [`docs/CLAUDE_HANDOFF_RECEIPT.md`](docs/CLAUDE_HANDOFF_RECEIPT.md)
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/SAFETY_AND_LIMITATIONS.md`](docs/SAFETY_AND_LIMITATIONS.md)
- [`docs/KOSHDB_ADAPTER_CONTRACT.md`](docs/KOSHDB_ADAPTER_CONTRACT.md)
- [`reports/KOSH_ADAPTER_GATE.md`](reports/KOSH_ADAPTER_GATE.md)
- [`docs/PLATFORM_SUPPORT.md`](docs/PLATFORM_SUPPORT.md)
- [`docs/VECTOR_INDEX.md`](docs/VECTOR_INDEX.md)
- [`reports/VECTOR_INDEX_RECALL.md`](reports/VECTOR_INDEX_RECALL.md)
- [`docs/GRAPHENE_LATTICE_MODEL.md`](docs/GRAPHENE_LATTICE_MODEL.md)
- [`docs/LATTICE_RETRIEVAL.md`](docs/LATTICE_RETRIEVAL.md)
- [`docs/RC4_REVIEWER_RESPONSE_REPORT.md`](docs/RC4_REVIEWER_RESPONSE_REPORT.md)

## API sketch

```cpp
#include "graphene/db.hpp"
using namespace graphene;

GrapheneDB db;
DBOptions opt;
opt.dimension = 768;
db.open("./memory.graphenedb", opt);

NodeInput n;
n.content = "Checkout timeout after GCP ingress change";
n.vector = embedding;
n.signature = signature_for(2, 7);
n.symptom = true;
n.metadata = {{"type", "incident"}, {"service", "checkout"}};
uint32_t node_id = 0;
db.put_node(n, &node_id);

auto bundle = db.causal_search(query_embedding, signature_for(2, 7), QueryMode::Empirical);
```

More examples are in [`examples/`](examples/).

## Current evidence snapshot

Current and historical evidence retained in `reports/`:

```text
Release CTest: 10/10 passed
Examples/demo: pass
100k stress: pass
ASAN/UBSAN selected gates: pass
TSAN selected gates: not completed in this sandbox due build/link timeout
```

Earlier RC2/RC3/RC4 reports are also retained for 1M storage, process-kill, fuzz smoke, portability, and soak evidence. RC5 adds lattice/extraction/packaging/operator-readiness artifacts, not a claim of 24-hour enterprise GA certification.

## Known limitations

- No distributed mode.
- No SQL.
- No persistent secondary metadata index files yet; metadata indexes are rebuilt in memory from durable node records.
- WAL retention is basic.
- Fuzz and soak gates are smoke-level in this pack, not long-running certification.
- KoshDB/LLM-Kosh integration is adapter-level and TSV interchange-based; live repo integration still needs the actual upstream repository/runtime.
- Vector search has a `VectorIndex` seam with configurable `Auto`, `Flat`, `KDTree`, and optional `Faiss` backends; signature-plane routing reduces causal candidate sets, but persisted HNSW/FAISS-class integration remains future work.

## License

Current license is a placeholder. Replace `LICENSE` before public distribution.
