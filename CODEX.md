# Codex Repository Guide

Use `AGENTS.md` as the authoritative instruction file and `README.md` as the operator quick start.

## Objective

Maintain GrapheneDB as a domain-neutral, provenance-first reasoning substrate. A successful resolved response must be produced through Graphene ingestion/model-world construction, HypoKosh iterative reasoning, dialectic expansion/opposition/convergence, and governed projection. Return both the concise answer and the validated evidence path.

## First commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/graphenedb_recursive_model_world_tests ./testdata
```

## Change protocol

1. Inspect the relevant public header, implementation and existing tests.
2. Add a failing test before changing reasoning or durable behaviour.
3. Avoid case-specific answer regexes when a typed relation or graph constraint can express the behaviour.
4. Preserve arbitrary predicates and token-tagged/structured input support.
5. Verify non-zero HypoKosh rounds and full execution attestation for reasoning tests.
6. Run focused tests, then the complete CTest suite.
7. Update README/docs and committed evidence only when results were actually reproduced.

## Never do

- bypass Graphene, HypoKosh or dialectic stages for convenience;
- return an evidence path in place of the requested answer;
- discard the reasoning path after projection;
- infer a factual edge from sentence adjacency alone;
- weaken provenance, temporal validity, contradiction or no-silent-promotion checks;
- claim GA, scale, soak, fuzz or benchmark results without artifacts.