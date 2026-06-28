# GrapheneDB v1 RC4

**GrapheneDB** is an experimental embedded **causal-memory database** for AI agents, coding assistants, incident-memory systems, and team-brain workflows.

It is not trying to replace SQLite, Qdrant, or Neo4j. Its niche is narrower:

> Store AI memory as versioned nodes, embeddings, metadata, and causal/contradiction/supersession edges, then retrieve an explainable evidence bundle rather than only a list of similar chunks.

## Status

**Current maturity:** v1 RC4 / controlled pilot + developer-review candidate.

RC4 builds on the RC3 productization pack and adds portability/core-debt improvements: platform abstraction, cleaner WAL frame construction, manifest-persisted txid, named retrieval tuning constants, cached live counts, std::popcount portability, and a VectorIndex seam.

It is not yet external enterprise GA. Remaining GA work is documented in [`docs/GA_READINESS_SCORECARD.md`](docs/GA_READINESS_SCORECARD.md).


## Platform support

| Platform | Status | Notes |
|---|---|---|
| Linux | Validated | Release build, CTest, examples, 100k stress, and ASAN/UBSAN selected gates passed in this environment. |
| macOS | Expected | POSIX platform layer should apply; included in CI matrix, but not validated in this sandbox. |
| Windows | Compile-target / smoke pending | RC4 adds a Windows platform layer and excludes POSIX-only process-kill tests. Windows was not locally compiled in this Linux sandbox. |

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
```

## One-command validation scripts

| Script | Purpose |
|---|---|
| `scripts/build_release.sh` | Configure and build release artifacts. |
| `scripts/run_all_tests.sh` | Build and run full CTest suite. |
| `scripts/run_sanitizers.sh` | Run selected ASAN/UBSAN and TSAN gates. |
| `scripts/run_100k_stress.sh` | Run 100k-class stress profile. |
| `scripts/run_1m_stress.sh` | Run 1M-node storage stress profile. |
| `scripts/run_crash_matrix.sh` | Run process-kill and recovery tests. |
| `scripts/run_fuzz_smoke.sh` | Build and run LLVM libFuzzer smoke target. |
| `scripts/run_examples.sh` | Run all API examples and demo programs. |
| `scripts/run_graphene_uniqueness_demo.sh` | Run the vector vs graph vs Graphene demo. |

## Key docs

- [`docs/GRAPHENE_UNIQUENESS.md`](docs/GRAPHENE_UNIQUENESS.md)
- [`docs/V1_RC_ACCEPTANCE_REPORT.md`](docs/V1_RC_ACCEPTANCE_REPORT.md)
- [`docs/GA_READINESS_SCORECARD.md`](docs/GA_READINESS_SCORECARD.md)
- [`docs/CLAUDE_HANDOFF_RECEIPT.md`](docs/CLAUDE_HANDOFF_RECEIPT.md)
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/SAFETY_AND_LIMITATIONS.md`](docs/SAFETY_AND_LIMITATIONS.md)
- [`docs/KOSHDB_ADAPTER_CONTRACT.md`](docs/KOSHDB_ADAPTER_CONTRACT.md)
- [`docs/PLATFORM_SUPPORT.md`](docs/PLATFORM_SUPPORT.md)
- [`docs/VECTOR_INDEX.md`](docs/VECTOR_INDEX.md)
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

RC4 evidence generated in this sandbox and retained in `reports/`:

```text
Release CTest: 10/10 passed
Examples/demo: pass
100k stress: pass
ASAN/UBSAN selected gates: pass
TSAN selected gates: not completed in this sandbox due build/link timeout
```

Earlier RC2/RC3 reports are also retained for 1M storage, process-kill, fuzz smoke, and soak evidence. RC4 adds portability/core-debt fixes and developer trust artifacts, not a claim of 24-hour enterprise GA certification.

## Known limitations

- No distributed mode.
- No SQL.
- No persistent secondary metadata index files yet; metadata indexes are rebuilt in memory from durable node records.
- WAL retention is basic.
- Fuzz and soak gates are smoke-level in this pack, not long-running certification.
- KoshDB/LLM-Kosh integration is adapter-level and TSV interchange-based; live repo integration still needs the actual upstream repository/runtime.
- Vector search has a `VectorIndex` seam and current `FlatVectorIndex` implementation; signature-plane routing reduces causal candidate sets, but HNSW/FAISS integration remains future work.

## License

Current license is a placeholder. Replace `LICENSE` before public distribution.
