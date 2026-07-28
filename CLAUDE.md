# Claude Code Repository Guide

Read `AGENTS.md` first. It contains the binding engineering and verification rules for this repository.

GrapheneDB is a C++20 embedded provenance-first causal/lattice-memory database plus an experimental reasoning stack. It supports ordinary text, JSON/JSONL, TSV, pipe-delimited triples, RDF-like records and token-tagged subject-predicate-object data. The relation vocabulary is domain-neutral.

## Required reasoning route

```text
Graphene ingestion and canonical relations
  -> Graphene model world
  -> HypoKosh iterative controller
  -> dialectic expansion
  -> dialectic opposition
  -> dialectic convergence
  -> governed answer projection
  -> answer + path + evidence + attestation
```

Do not introduce shortcuts around this route. Preserve abstention when evidence is insufficient.

## Build before editing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

## Primary files

- `src/text_model_world.cpp`
- `src/recursive_model_world.cpp`
- `src/dialectic.cpp`
- `include/graphene/text_model_world.hpp`
- `include/graphene/recursive_model_world.hpp`
- `tests/test_recursive_model_world.cpp`

## Completion criteria

A reasoning change is complete only when focused and full tests pass, answer and ordered relation path are asserted, full-pipeline attestation is true, non-zero reasoning rounds are observed, and unsupported data still abstains safely.