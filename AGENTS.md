# GrapheneDB Agent Instructions

GrapheneDB is a C++20 embedded provenance-first causal/lattice-memory database with an optional controlled-pilot HTTP server. The current developer-preview line also contains Graphene text/model-world ingestion, HypoKosh iterative reasoning, dialectic expansion/opposition/convergence, governed answer projection, and a domain-neutral canonical relation pipeline for structured and token-tagged datasets.

## Product boundary

GrapheneDB is not a SQL database, distributed cluster, general vector-database replacement, or material-science simulator. The embedded storage engine is authoritative. The HTTP server is optional and must remain a narrow versioned wrapper over tested core APIs.

The reasoning contract is:

```text
input data
  -> Graphene atomisation and canonical relation emission
  -> Graphene model world
  -> HypoKosh iterative path planning
  -> dialectic expansion, opposition and convergence
  -> governed answer projection
  -> answer + reasoning path + source evidence + execution attestation
```

Do not add an early answer-return shortcut that bypasses any of these stages. Preserve the reasoning path; it is not interchangeable with the concise answer.

## Read first

1. `README.md`
2. `docs/ARCHITECTURE.md`
3. `docs/STORAGE_FORMAT.md`
4. `docs/DIALECTIC_REASONING_V0.md`
5. `docs/EXTRACTION_INGESTION.md`
6. `updates/generic-data/GRAPHENEDB_GENERIC_DATA_PIPELINE_REPORT.md`
7. `updates/generic-data/GRAPHENEDB_GENERIC_DATASET_METRICS.json`

## Code map

- `include/graphene/`, `src/` — public API and implementation
- `src/text_model_world.cpp` — text atomisation and canonical relation extraction
- `src/recursive_model_world.cpp` — HypoKosh iterative controller, typed path reasoning and governed projection
- `src/dialectic.cpp` — dialectic reasoning stages
- `tools/graphenedb_cli.cpp` — CLI
- `tools/graphenedb_server.cpp` — optional HTTP server
- `tests/test_recursive_model_world.cpp` — generic, tokenised and full-pipeline assertions
- `bench/` — explicit benchmarks
- `scripts/` — build, stress, release and evidence workflows

## Non-negotiable rules

- Preserve durable-format compatibility. Document and test every storage-format change.
- Keep the embedded library authoritative; do not duplicate database logic in the server.
- Preserve no-silent-promotion and evidence-required/HITL behaviour.
- Every resolved reasoning answer must retain a source-grounded semantic path.
- Adjacency may help parse context but cannot be the sole factual edge in a resolved path.
- Domain-neutral predicates must remain supported; do not regress to a hard-coded human relationship ontology.
- Structured and token-tagged inputs must preserve punctuation, decimals and original predicate wording.
- Public claims must match committed evidence. This is a developer preview, not enterprise GA.
- Do not commit build directories, temporary databases, credentials or scratch outputs.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Focused reasoning verification:

```bash
./build/graphenedb_recursive_model_world_tests ./testdata
```

Controlled-pilot server verification:

```bash
cmake -S . -B build-server -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build-server -j2 --target graphenedb_server
python3 scripts/server_pilot_contract_test.py ./build-server/graphenedb_server
```

Larger stress, soak, fuzz and target-host gates remain separate from default CTest. Do not claim they ran unless their evidence was actually produced.