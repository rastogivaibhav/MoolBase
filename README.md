# GrapheneDB Developer Preview

GrapheneDB is an experimental C++20 embedded, provenance-first causal/lattice-memory database for AI agents, incident investigation, data lineage, research-pack ingestion, and auditable enterprise reasoning.

It stores versioned nodes, vectors, metadata, typed edges, source evidence, contradiction and supersession signals, then returns a concise answer together with the exact evidence-backed reasoning path that produced it.

> Current status: developer preview / controlled pilot. This is not enterprise GA.

## Important branch status

The `codex/generic-data-tokenized` branch contains the latest generic/tokenised reasoning implementation as a committed unified source patch plus its frozen metrics, validation report, and operator/agent documentation. The tested full source snapshot is also available as the release handoff artifact associated with this work.

The branch is not yet a fully materialised replacement of every file from that tested source snapshot. Before merging, apply and review `updates/generic-data/GRAPHENEDB_GENERIC_DATA_TOKENIZED.patch` against the preceding canonical-relation source line, then run the full build and CI suite. See `updates/generic-data/REMOTE_SOURCE_STATUS.md`.

## What is included

- Embedded C++20 storage engine and CLI
- Optional controlled-pilot HTTP server
- Versioned memory nodes, vectors, metadata, causal and semantic edges
- Checksummed WAL, replay, checkpointing, backup, compaction and stale-lock recovery
- Graphene-inspired hexagonal lattice topology and lattice-aware retrieval
- Text atomisation and canonical relation extraction
- Domain-neutral structured relation ingestion
- HypoKosh iterative path planning
- Dialectic expansion, opposition and convergence
- Governed answer projection with no-silent-promotion and evidence-required states
- Answer, ordered reasoning path, source evidence and execution attestation

## Domain-neutral data

The reasoning layer is not limited to human relationships. It supports arbitrary typed predicates across business, scientific, software, telemetry, healthcare, manufacturing and tokenised datasets.

Supported representations include:

- ordinary text
- JSON and JSONL edge records
- TSV and pipe-delimited triples
- RDF/N-Triples-style subject-predicate-object records
- token-tagged records using `SUBJ/REL/OBJ` or `S/P/O`

A generic path request is represented as a start entity and an ordered relation path:

```json
{
  "start": "portal-ui",
  "relations": ["calls", "reads_from", "hosted_in"],
  "terminal_type": "region"
}
```

A resolved response keeps the answer and path separate:

```json
{
  "answer": "gcp-europe-west2",
  "status": "resolved",
  "reasoning_path": [
    {"from":"portal-ui","relation":"calls","to":"catalog-api"},
    {"from":"catalog-api","relation":"reads_from","to":"product-db"},
    {"from":"product-db","relation":"hosted_in","to":"gcp-europe-west2"}
  ],
  "full_pipeline_complete": true,
  "canonical_relation_pipeline_complete": true
}
```

`full_pipeline_complete` is true only after the following stages execute:

```text
input data
  -> Graphene atomisation and canonical relation emission
  -> Graphene model world
  -> HypoKosh iterative controller and path planning
  -> dialectic expansion
  -> dialectic opposition
  -> dialectic convergence
  -> governed answer projection
  -> answer + path + evidence + attestation
```

The system must not return an early answer that bypasses these stages.

## Prerequisites

Recommended Linux setup:

- CMake 3.20+
- C++20 compiler: GCC 11+, Clang 14+, or equivalent
- Python 3.10+ for harnesses and contract tests
- Git

Optional model-backed proposal experiments additionally use `scikit-learn` and `joblib`.

## Build and test

```bash
git clone https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
git checkout codex/generic-data-tokenized

# Materialise the latest generic-data patch against the canonical-relation source line before building.
# Review the patch paths and strip level in your working tree rather than applying it blindly.

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF

cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Focused reasoning verification:

```bash
./build/graphenedb_recursive_model_world_tests ./testdata
```

The latest verified local run passed all 45 configured CTest cases. The committed metrics and report are in:

- `updates/generic-data/GRAPHENEDB_GENERIC_DATASET_METRICS.json`
- `updates/generic-data/GRAPHENEDB_GENERIC_DATA_PIPELINE_REPORT.md`
- `updates/generic-data/GRAPHENEDB_GENERIC_DATA_TOKENIZED.patch`

## Embedded CLI smoke test

```bash
./build/graphenedb_cli init /tmp/gdb-demo 3
./build/graphenedb_cli put-node /tmp/gdb-demo 3 "root cause" 0.9,0.1,0.0 131074 root
./build/graphenedb_cli put-node /tmp/gdb-demo 3 "checkout timeout" 0.1,0.9,0.0 131074 symptom
./build/graphenedb_cli put-edge /tmp/gdb-demo 3 0 1 causal
./build/graphenedb_cli search /tmp/gdb-demo 3 0.1,0.9,0.0 131074
./build/graphenedb_cli validate /tmp/gdb-demo 3
```

Automation-friendly commands support `--json`, including `inspect` and `validate`.

## Reason over text

```bash
./build/graphenedb_cli reason-text testdata/holdout/dialogue_paraphrase.txt \
  "Why was the external go-live deferred?" \
  --max-rounds 6 \
  --json
```

The JSON response should be inspected for:

- final answer and status
- ordered reasoning path
- source evidence
- non-zero HypoKosh rounds
- Graphene, HypoKosh and dialectic execution attestation
- governed projection outcome

## Optional pilot HTTP server

```bash
cmake -S . -B build-server \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF

cmake --build build-server -j2 --target graphenedb_server
export GRAPHENEDB_API_KEY=development-key
./build-server/graphenedb_server /tmp/graphenedb 64 8080
```

Discover the API with `GET /v1/version`. For any non-loopback deployment, place the server behind the documented TLS reverse proxy and follow the security guidance.

## Repository orientation

- `include/graphene/` — public C and C++ headers
- `src/` — storage, Graphene, HypoKosh, dialectic and governed reasoning implementation
- `tools/graphenedb_cli.cpp` — CLI
- `tools/graphenedb_server.cpp` — optional server
- `tests/` — unit, durability, crash, lattice, extraction and reasoning tests
- `bench/` — explicit benchmark programs
- `scripts/` — build, stress, recovery, release and evidence workflows
- `docs/` — architecture, storage, security and operator documentation
- `updates/generic-data/` — generic/tokenised reasoning patch and validation evidence

AI coding agents should read `AGENTS.md` first. Codex-specific guidance is in `CODEX.md`; Claude Code guidance is in `CLAUDE.md`.

## Engineering constraints

- Preserve durable-format compatibility and document storage-format changes.
- Keep the embedded library authoritative; server endpoints must call tested core APIs.
- Preserve no-silent-promotion, contradiction handling, temporal validity and abstention.
- Retain the semantic reasoning path after projecting the concise answer.
- Do not infer a factual edge from sentence adjacency alone.
- Do not regress arbitrary predicates into a fixed human-relationship ontology.
- Do not claim long soak, fuzz, target-scale or enterprise-GA evidence unless those gates were actually run and preserved.

## Current limitations

- No distributed mode or SQL interface
- Developer preview rather than enterprise GA
- Generic relation extraction remains deterministic and pattern-oriented for some unstructured inputs
- Broader ontology mediation, entity resolution and confidence calibration remain roadmap work
- Long-duration 24h/72h soak and target-host certification remain separate release gates

## License

Review and replace the current placeholder licence before unrestricted public distribution.