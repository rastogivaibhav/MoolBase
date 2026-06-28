# Claude / engineer handoff receipt

## Objective

Take GrapheneDB from RC3 productization pack to either:

1. **v1.0 controlled pilot release**, or
2. **re-scoped Graphene retrieval sidecar**, if extended evidence does not justify DB positioning.

## Current artifact

`graphenedb_v1_rc3_productization_pack.zip`

## What this repository contains

- Embedded C++20 library: `include/graphene`, `src`.
- CLI: `tools/graphenedb_cli.cpp`.
- Tests: `tests/`.
- Benchmarks: `bench/`.
- Fuzzer target: `fuzz/fuzz_wal_open.cpp`.
- Examples and uniqueness demo: `examples/`.
- One-command scripts: `scripts/`.
- GitHub Actions CI: `.github/workflows/ci.yml`.
- Acceptance and readiness docs: `docs/`.

## First commands to run

```bash
./scripts/build_release.sh
./scripts/run_all_tests.sh
./scripts/run_graphene_uniqueness_demo.sh
./scripts/run_sanitizers.sh
```

Optional heavier gates:

```bash
./scripts/run_100k_stress.sh
./scripts/run_1m_stress.sh
./scripts/run_crash_matrix.sh
RUNS=10000 ./scripts/run_fuzz_smoke.sh
```

## Most important files

| File | Why it matters |
|---|---|
| `include/graphene/db.hpp` | Public DB API. |
| `include/graphene/types.hpp` | Node, edge, query, status, and memory bundle types. |
| `src/db.cpp` | Storage, WAL, replay, search, compaction, locking. |
| `include/graphene/kosh_adapter.hpp` | Adapter boundary for KoshDB/LLM-Kosh. |
| `examples/graphene_uniqueness_demo.cpp` | Demonstrates why Graphene is not just vector search. |
| `docs/GA_READINESS_SCORECARD.md` | Remaining work before GA. |
| `docs/V1_RC_ACCEPTANCE_REPORT.md` | What is proven vs not proven. |

## Known risk areas to inspect first

1. WAL/checkpoint interaction under crash.
2. Lock-file handling across OSes.
3. Metadata index rebuild cost at high scale.
4. Causal search selectivity on messy real-world data.
5. Flat vector search cost at high dimensions.
6. Lack of batch transaction API.
7. Live KoshDB/LLM-Kosh integration.

## Recommended next PRs

### PR 1 — Batch transaction API

Add:

```cpp
begin_write();
put_node(tx, ...);
put_edge(tx, ...);
commit(tx);
rollback(tx);
```

### PR 2 — Live KoshDB integration

Replace TSV-only gate with real import/export against the actual KoshDB/LLM-Kosh repository.

### PR 3 — Long-running evidence

Run and store artifacts for:

- 24-hour soak
- multi-hour fuzz
- realistic 1M graph/vector benchmark

### PR 4 — Query evidence benchmark

Build a dataset where vector-only, graph-only, and Graphene retrieval can be compared on answer quality.

## Honest release language

Use:

> GrapheneDB v1 RC3 is an embedded causal-memory database candidate for controlled AI memory pilots.

Do not use yet:

> GrapheneDB is production-proven enterprise GA or a Qdrant/Neo4j/SQLite replacement.
