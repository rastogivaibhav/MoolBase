# GrapheneDB + TheHypoKosh Discrete Lyapunov Runtime

GrapheneDB is an experimental embedded C++ causal/lattice-memory database. This developer-preview branch materialises the bounded TheHypoKosh reasoning loop and replaces the earlier heuristic-only runtime critic with a discrete Lyapunov analysis layer.

## Maturity

**Experimental runtime / developer preview. Not enterprise GA.**

## Five-minute start

Clone the developer-preview branch and run the disposable reasoning demo:

```bash
git clone --branch benchmark/cross-dataset-epistemic-suite --single-branch \
  https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
scripts/developer_quickstart.sh
```

Windows PowerShell:

```powershell
.\scripts\developer_quickstart.ps1
```

To prove that GrapheneDB can be installed and consumed from an unrelated CMake project:

```bash
scripts/verify_developer_install.sh
```

That verification performs a clean build, installs GrapheneDB into a temporary prefix, builds `examples/installed_consumer` with `find_package(GrapheneDB CONFIG REQUIRED)`, and runs the external consumer. See [`docs/DEVELOPER_QUICKSTART.md`](docs/DEVELOPER_QUICKSTART.md) for prerequisites, expected output and next steps.

The executable reasoning path is:

```text
Graphene expansion
→ immutable FiberBundle
→ discrete Lyapunov critic
→ bounded CorrectiveEscape
→ convergence
→ opposition
→ bounded re-expansion
→ governed answer projection
→ model-world event
```

The Lyapunov critic certifies or rejects **observed practical stability over the finite recorded reasoning trajectory**. It is not a proof of global asymptotic stability for every possible future graph state or external discovery action.

## Complete critic

The critic constructs a seven-dimensional bounded epistemic error state from:

- temporal deficit;
- path-diversity deficit;
- independent-path degeneracy deficit;
- provenance deficit;
- contradiction excess;
- premature-pattern-lock excess;
- missing-evidence excess.

It evaluates the positive weighted quadratic candidate:

```text
V(x) = sum(w_i x_i²) / sum(w_i)
```

and reports:

- lower and upper quadratic coefficients;
- per-cycle energy and drift;
- sufficient-decrease checks;
- mean and worst contraction ratios;
- maximum observed energy increase;
- goal-set arrival and equilibrium dwell;
- practical-stability and convergence certificates;
- divergence, oscillation and repeated-state limit-cycle detection;
- explicit certificate violations.

Empirical, balanced and theoretical modes use different target sets and weights. `StabilityCriticV0` remains as a compatibility facade, but the complete runtime uses `LyapunovCritic`.

See [`docs/LYAPUNOV_CRITIC_V1.md`](docs/LYAPUNOV_CRITIC_V1.md).

## Other runtime capabilities

- deterministic immutable `FiberBundle`;
- exact duplicate-path removal;
- source-lineage-aware path independence;
- temporal and provenance-bearing evidence paths;
- contradiction and supersession handling;
- atomic all-source hyperedges;
- bounded corrective escape and falsification questions;
- convergence without deleting the original bundle;
- bounded opposition and re-expansion;
- governed statuses: `resolved`, `provisionally_resolved`, `contested`, `evidence_required`, `abstain`, `speculative`;
- no-silent-promotion enforcement;
- bounded self-healing proposals;
- persistent checksummed model-world ledger and audits;
- relation ontology and ambiguity-preserving entity resolution;
- TSV, pipe, RDF-like and token-tagged structured-relation ingestion;
- CLI reasoning;
- authenticated POSIX HTTP reasoning endpoint.

The source-to-test mapping is in [`docs/PAPER_THESIS_IMPLEMENTATION_MATRIX.md`](docs/PAPER_THESIS_IMPLEMENTATION_MATRIX.md).

## Build and test

For a first local build, keep the optional server disabled:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Verified exact split-source integration result:

```text
45/45 tests passed
0 failures
23.65 seconds
```

A fresh extraction of the final source package configured and built successfully. All 45 tests passed; the container execution ceiling required tests 38–45 to be resumed in a second bounded CTest invocation.

Coverage includes storage, C API, WAL/crash/fault handling, stale locks, lattice storage and traversal, one-million-record smoke, extraction ingestion, dialectic reasoning, hyperedges, governed learning, generic relation reasoning, CLI reasoning, complete runtime integration, adversarial critic cases, the direct Lyapunov suite, OpenAPI validation and live HTTP contracts.

See [`reports/LYAPUNOV_CRITIC_V1_BUILD_TEST_REPORT.md`](reports/LYAPUNOV_CRITIC_V1_BUILD_TEST_REPORT.md).

## CLI

```bash
./build/graphenedb_cli reason /tmp/graphenedb 16 <comma-vector> <signature> \
  --mode empirical --max-rounds 3 --json
```

The output includes the governed status, bundle hashes, initial/final Lyapunov energy, final regime and certificate flags.

## HTTP

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

The response includes the complete trajectory, energy drift, quadratic bounds, stability certificate, opposition, self-healing plan, evidence edges and execution receipt.

## Main implementation files

```text
include/graphene/stability_critic.hpp
include/graphene/hypokosh_runtime.hpp
src/stability_critic.cpp
src/stability_critic_part_*.inc
src/hypokosh_runtime.cpp
src/hypokosh_runtime_part_*.inc
src/escape.cpp
src/self_healing.cpp
tests/test_lyapunov_critic.cpp
tests/test_hypokosh_runtime.cpp
scripts/server_hypokosh_runtime_contract_test.py
```

Large translation units use small deterministic wrapper files plus `.inc` fragments so the complete source is reviewable and transportable without an unapplied patch.

## Honest limitations

- no global asymptotic-stability proof for the unbounded model world;
- no official external benchmark result yet;
- model world is a local bounded ledger, not a distributed autonomous scheduler;
- generic parser is bounded and is not a general natural-language semantic parser;
- POSIX HTTP server is unsupported on Windows;
- separate Windows stale-lock work still needs review and clean Windows validation;
- long-duration production-hardware soak and security/release certification remain open gates.
