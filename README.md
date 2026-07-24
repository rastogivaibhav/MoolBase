# GrapheneDB v0.6.0-rc1

**GrapheneDB** is an experimental embedded **causal/lattice-memory database** for AI agents, coding assistants, incident-memory systems, research-pack ingestion, and team-brain workflows.

It is not trying to replace SQLite, Qdrant, or Neo4j. Its niche is narrower:

> Store AI memory as versioned nodes, embeddings, metadata, and causal/contradiction/supersession edges, then retrieve an explainable evidence bundle rather than only a list of similar chunks.

## Status

**Current maturity:** v0.6.0-rc1 / controlled private-pilot release candidate.

v0.6.0-rc1 keeps the embedded C++ engine and adds an optional hardened HTTP server for controlled pilots: bounded workers, rate limiting, structured telemetry, strict request framing, version discovery, retry-safe node writes, graceful checkpointing shutdown, and an atomic bounded bulk path.

It is not yet external enterprise GA. Remaining GA work is documented in [`docs/GA_READINESS_SCORECARD.md`](docs/GA_READINESS_SCORECARD.md), with the execution plan in [`docs/NEXT_GA_EXECUTION_PLAN.md`](docs/NEXT_GA_EXECUTION_PLAN.md).

This branch also adds the first durable graphene-inspired lattice model: hexagonal node coordinates, lattice bond/defect metadata, topology validation, and optional lattice-aware retrieval propagation. The model is a memory topology and retrieval primitive, not a carbon-physics or material-science simulator.

It also adds a first-class extraction ingestion API for source-scoped external
IDs, conflict-safe idempotent re-import, relation resolution, automatic
lattice placement, CLI ingestion, and bounded atomic HTTP ingestion at
`POST /v1/extractions`. See
[`docs/EXTRACTION_INGESTION.md`](docs/EXTRACTION_INGESTION.md).

Docker/Linux contract evidence and the pinned public-postmortem comparison are
documented in
[`docs/REAL_DATA_DOCKER_VALIDATION.md`](docs/REAL_DATA_DOCKER_VALIDATION.md).

This development branch also contains the experimental `GDB-GL-0` governed
learning mechanism: read-only HypoKosh proposals, immutable verified outcome
episodes, deterministic data-utility measurement, offline retrieval-policy
comparison, and explicitly approved promotion/rollback. It does not train LLM
weights or autonomously promote facts or policies. See
[`docs/GOVERNED_LEARNING_V0_SPEC.md`](docs/GOVERNED_LEARNING_V0_SPEC.md).
The completed controlled-pilot mechanism evidence is recorded in
[`docs/GOVERNED_LEARNING_V0_VALIDATION.md`](docs/GOVERNED_LEARNING_V0_VALIDATION.md).


## Platform support

| Platform | Status | Notes |
|---|---|---|
| Linux | CI target | Release build/test, package smoke, fuzz/sanitizer smoke, and GA readiness smoke are configured in CI; preserve CI artifacts for release evidence. |
| macOS | CI target | POSIX platform layer should apply; CI matrix includes macOS build/test. |
| Windows | Embedded/CLI smoke validated with policy caveat | `GRAPHENEDB_BUILD_SERVER` defaults to `OFF`, allowing normal embedded/CLI builds and tests without compiling the POSIX server. The server remains unsupported on Windows and an explicit `ON` configuration fails during CMake configuration. `graphenedb_cli_extract_tests` runs through a Python harness on Windows. |

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
- Experimental bounded dialectic reasoning with typed RFC3339 validity,
  provenance findings, atomic all-source hyperedges, deterministic opposition,
  and a read-only authenticated pilot endpoint
- Experimental read-only HypoKosh proposals and governed outcome learning for
  retrieval-policy recommendations, with held-out evaluation, safety-negative
  episodes, explicit approval, rollback, quarantine, and restart-safe history

## Pilot server quick start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2 --target graphenedb_server
export GRAPHENEDB_API_KEY=development-key
./build/graphenedb_server /tmp/graphenedb 64 8080 \
  --physical-lattice-primary --physical-lattice-radius 256 \
  --expected-max-nodes 197377 --wal-rotate-bytes 268435456
```

Discover the contract with `GET /v1/version`. The OpenAPI document is in [`docs/api/openapi-v1.yaml`](docs/api/openapi-v1.yaml), and the dependency-free Python client is in [`clients/python`](clients/python). Put the server behind the documented TLS reverse proxy before any non-loopback deployment.

## Embedded quick start

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

GrapheneDB is a compact embedded C++ database with an optional controlled-pilot HTTP server; the embedded library remains the canonical storage engine. The core code lives in `include/graphene/` and `src/`, with a CLI in `tools/graphenedb_cli.cpp`, C ABI glue in `include/graphene/c_api.h` and `src/c_api.cpp`, benchmarks in `bench/`, and release/operator automation in `scripts/`.

The fastest way to understand the repository is:

1. Read this README for product scope and maturity.
2. Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for storage, WAL, snapshots, retrieval, and CLI structure.
3. Read [`docs/GRAPHENE_LATTICE_MODEL.md`](docs/GRAPHENE_LATTICE_MODEL.md), [`docs/LATTICE_RETRIEVAL.md`](docs/LATTICE_RETRIEVAL.md), and [`docs/EXTRACTION_INGESTION.md`](docs/EXTRACTION_INGESTION.md) for the RC additions.
4. Read [`docs/NEXT_GA_EXECUTION_PLAN.md`](docs/NEXT_GA_EXECUTION_PLAN.md) and [`reports/GA_STATUS_REPORT.md`](reports/GA_STATUS_REPORT.md) before changing release claims.

Developer rules of thumb:

- Preserve the embedded library as the canonical engine. Keep the optional HTTP server narrow, versioned, reverse-proxy-oriented, and free of distributed-cluster or SQL scope.
- Keep durable format changes intentional and documented in [`docs/STORAGE_FORMAT.md`](docs/STORAGE_FORMAT.md), with fixtures or migration tests where applicable.
- Treat `put_batch()` and `put_extraction()` as public API surface; add tests before changing semantics.
- Prefer deterministic tests and preserved report output for release gates.
- Generated build directories are ignored. Preserved release evidence belongs under `reports/` only when it is intentionally part of a release candidate.

Agent/developer orientation files are provided at the repository root:

- [`CLAUDE.md`](CLAUDE.md) for Claude-style coding agents.
- [`AGENTS.md`](AGENTS.md) for Codex-style coding agents.

Handoff next step: keep the embedded library authoritative, treat `v0.6.0-rc1` as a controlled-pilot release candidate rather than public GA, and focus the next iteration on evidence gates instead of new features. The immediate remaining gates are the 24-hour and 72-hour soak runs on intended hardware/filesystem, real OCI/SBOM/vulnerability-scan evidence, resolution of the slow 5,000-incident stress profile and higher-load soak shutdown issue, and replacement of the placeholder licence before any broader distribution.

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
| `scripts/server_learning_contract_test.py` | Exercise the authenticated governed-learning loop, promotion, rollback, and restart contract. |
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

- [`deepmindtest.md`](deepmindtest.md) — preregistered dialectic, HypoKosh,
  learning, safety, and scale benchmark with explicit acceptance gates.
- [`docs/DEEPMIND_G0_G2_TESTS.md`](docs/DEEPMIND_G0_G2_TESTS.md) — executable
  clean-run manifest and deterministic D0/G2 gate.
- [`docs/GRAPHENE_UNIQUENESS.md`](docs/GRAPHENE_UNIQUENESS.md)
- [`docs/GRAPHENE_LATTICE_MODEL.md`](docs/GRAPHENE_LATTICE_MODEL.md)
- [`docs/LATTICE_RETRIEVAL.md`](docs/LATTICE_RETRIEVAL.md)
- [`docs/DIALECTIC_REASONING_V0.md`](docs/DIALECTIC_REASONING_V0.md)
- [`docs/GOVERNED_LEARNING_V0_SPEC.md`](docs/GOVERNED_LEARNING_V0_SPEC.md)
- [`docs/GOVERNED_LEARNING_V0_VALIDATION.md`](docs/GOVERNED_LEARNING_V0_VALIDATION.md)
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

## Hardened server pilot

The optional HTTP server now uses a bounded worker pool, bounded queue, token-bucket rate limiting, request-size limits, structured JSON logs, Prometheus metrics, and physical-lattice capacity checks.

```bash
export GRAPHENEDB_API_KEY='replace-with-a-random-secret'
./build/graphenedb_server /var/lib/graphenedb 64 8080 \
  --physical-lattice-primary \
  --physical-lattice-radius 600 \
  --expected-max-nodes 1000000 \
  --wal-rotate-bytes 268435456 \
  --workers 8 --queue-capacity 1024 \
  --rate-limit-rps 200 --rate-limit-burst 400
```

Useful operational endpoints:

- `GET /v1/health` — liveness, unauthenticated.
- `GET /v1/ready` — readiness, unauthenticated and count-free.
- `GET /v1/metrics` — authenticated JSON metrics.
- `GET /v1/metrics/prometheus` — authenticated Prometheus exposition.
- `GET /v1/admin/capacity` — physical radius/capacity/utilisation.

Do not expose the plain HTTP server directly to the internet. See `docs/SERVER_DEPLOYMENT_SECURITY.md`, `docs/PHYSICAL_LATTICE_CAPACITY_PLANNING.md`, and `docker-compose.secure.yml`.

Launch validation commands:

```bash
python3 scripts/server_launch_hardening_test.py ./build/graphenedb_server
scripts/run_adverse_filesystem_tests.sh build
python3 scripts/server_soak_test.py --binary ./build/graphenedb_server --seconds 86400 --clients 8 --target-rps 80
python3 scripts/validate_docker_security.py .
```

One-command controlled launch gate (defaults to a 5-minute soak):

```bash
SOAK_SECONDS=300 scripts/run_launch_readiness_gate.sh
```

The release environment must also run the real container build and CVE gate:

```bash
scripts/run_container_security_gate.sh
```

Set `SOAK_SECONDS=86400` on approved hardware for the 24-hour certification run.
