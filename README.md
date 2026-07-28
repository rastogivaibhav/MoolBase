# GrapheneDB + TheHypoKosh Experimental Runtime

GrapheneDB is an experimental embedded C++ causal/lattice-memory database. This branch materialises the bounded TheHypoKosh reasoning thesis as executable source rather than documentation-only claims.

## Maturity

**Experimental runtime / developer-preview candidate. Not enterprise GA.**

The implemented path is:

```text
Graphene expansion
→ immutable FiberBundle
→ StabilityCriticV0
→ bounded CorrectiveEscape
→ convergence
→ opposition
→ bounded re-expansion
→ governed answer projection
→ model-world event
```

`StabilityCriticV0` is inspired by Lyapunov-style stability analysis, but it is **not claimed to be a mathematically proven Lyapunov function**.

## Materialised capabilities

- temporal and provenance-bearing graph facts and edges
- causal, supporting, contradicting and superseding relationships
- atomic all-source hyperedges
- deterministic multi-path `FiberBundle`
- exact duplicate-path removal
- source-lineage-aware independent-path degeneracy
- temporal, diversity, provenance, contradiction and pattern-lock scoring
- bounded corrective escape and missing-evidence planning
- convergent compression without mutating the original bundle
- opposition, falsification questions and bounded re-expansion
- governed statuses: `resolved`, `provisionally_resolved`, `contested`, `evidence_required`, `abstain`, `speculative`
- no-silent-promotion checks
- bounded self-healing proposals
- persistent checksummed model-world ledger and audit scheduler
- empirical, balanced and theoretical reasoning modes
- relation ontology and ambiguity-preserving entity resolution
- TSV, pipe, RDF-like and `SUBJ/REL/OBJ` structured-relation ingestion
- CLI reasoning command
- authenticated POSIX HTTP runtime endpoint

The source-to-test contract is in [`docs/PAPER_THESIS_IMPLEMENTATION_MATRIX.md`](docs/PAPER_THESIS_IMPLEMENTATION_MATRIX.md).

## Build and test

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Verified Linux result for the exact packaged source:

```text
44/44 tests passed
0 failures
final exact-source run: 21.13 seconds
fresh extracted-package run: 20.65 seconds
```

The suite covers storage, crash/fault handling, lattice behaviour, one-million-record smoke, dialectic reasoning, hyperedges, governed learning, FiberBundle, StabilityCriticV0 adversarial cases, generic reasoning ingestion, CLI execution, OpenAPI surface validation and the live HTTP runtime contract.

See [`reports/HYPOKOSH_RUNTIME_V1_BUILD_TEST_REPORT.md`](reports/HYPOKOSH_RUNTIME_V1_BUILD_TEST_REPORT.md) and [`reports/HYPOKOSH_RUNTIME_V1_FINAL_CTEST.txt`](reports/HYPOKOSH_RUNTIME_V1_FINAL_CTEST.txt).

## CLI

After creating or ingesting a database:

```bash
./build/graphenedb_cli reason /tmp/graphenedb 16 <comma-vector> <signature> \
  --mode empirical --max-rounds 3 --json
```

The response includes the governed status, primary node, confidence, immutable initial/final bundle hashes, executed layers and no-silent-promotion result.

## HTTP runtime

```bash
export GRAPHENEDB_API_KEY='development-key'
./build/graphenedb_server /tmp/graphenedb 16 8080 \
  --bind-address 127.0.0.1 --workers 2 --queue-capacity 32
```

Invoke:

```http
POST /v1/reason/runtime
Content-Type: application/json
X-API-Key: development-key
```

Example request:

```json
{
  "query": "Why did checkout failures increase?",
  "signature": 33,
  "mode": "empirical",
  "max_hops": 5,
  "max_paths": 32,
  "max_recursive_cycles": 2
}
```

The endpoint returns evidence edges, stability metrics, opposition, self-healing proposals and a deterministic execution receipt. The API supplement is in [`docs/api/openapi-v1-runtime.yaml`](docs/api/openapi-v1-runtime.yaml).

## Main source locations

```text
include/graphene/fiber_bundle.hpp
include/graphene/stability_critic.hpp
include/graphene/escape.hpp
include/graphene/hypokosh_runtime.hpp
include/graphene/model_world.hpp
include/graphene/self_healing.hpp
include/graphene/relation_ontology.hpp
include/graphene/entity_resolution.hpp
include/graphene/generic_relation.hpp

src/fiber_bundle.cpp
src/stability_critic.cpp
src/escape.cpp
src/hypokosh_runtime.cpp
src/model_world.cpp
src/self_healing.cpp
src/relation_ontology.cpp
src/entity_resolution.cpp
src/generic_relation.cpp
```

The large CLI and POSIX server translation units are stored as small wrapper files plus deterministic `.inc` fragments so the complete source can be reviewed and transported without relying on an unapplied patch.

## Honest limitations

- no formal proof that `StabilityCriticV0` is a Lyapunov function
- no official external benchmark result yet
- model world is a local bounded ledger, not a distributed autonomous million-node scheduler
- structured-relation parser is bounded and is not a general natural-language semantic parser
- POSIX HTTP server is unsupported on Windows
- the separate Antigravity Windows stale-lock candidate is not included and still requires review and clean Windows validation
- long-duration production-hardware soak, external benchmark comparison and release/security certification remain open gates

No capability should be claimed from a paper or report unless its source and direct test appear in the implementation matrix.
