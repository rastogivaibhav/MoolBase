# GrapheneDB v0.6.0-alpha.1

GrapheneDB is an experimental embedded C++ evidence and causal-memory database for agentic systems. It combines typed graph retrieval with lineage-aware FiberBundles, a Lyapunov-inspired stability critic and governed decisions such as answer, deepen, contest or abstain.

## Maturity

**Experimental developer alpha for research and controlled pilots. Not enterprise GA and not a semantic truth engine.**

## Five-minute start

Clone the consolidated release branch and run the disposable reasoning demo:

```bash
git clone --branch release/v0.6.0-alpha.1 --single-branch \
  https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
bash scripts/developer_quickstart.sh
```

Windows PowerShell:

```powershell
.\scripts\developer_quickstart.ps1
```

Verify installation from an unrelated CMake project:

```bash
bash scripts/verify_developer_install.sh
```

Run the complete exact-head alpha gate:

```bash
bash scripts/run_alpha_release_gate.sh
```

The alpha gate performs a clean build, full CTest run, installed-package consumer test, controlled dialectic intervention benchmark, offline cross-dataset structural gate and immutable evidence-manifest generation.

See [`docs/DEVELOPER_QUICKSTART.md`](docs/DEVELOPER_QUICKSTART.md) for prerequisites, expected output and next steps.

## Runtime path

```text
Graphene expansion
→ immutable FiberBundle v2
→ semantic-verifier boundary
→ epistemic admissibility
→ Lyapunov-inspired stability critic
→ convergence and opposition
→ targeted recovery when evidence is incomplete
→ optional opposition-led secondary research
→ governed answer projection
→ compact epistemic receipt
→ selective model-world event
```

Recursive search is not always on. By default, the runtime performs one bounded pass and re-expands only for a graph-searchable defect such as a missing hop, insufficient independent evidence, contradiction, temporal mismatch, retrieval noise or a relevant minority path. Opposition-only secondary or tertiary research is opt-in.

## Main capabilities

- deterministic immutable `FiberBundle` schema v2;
- separation of graph-route, source, evidence-family and derivation lineage;
- exact duplicate-path removal and correlated-evidence grouping;
- support, opposition and noise kept as distinct epistemic roles;
- target-scoped semantic-verifier interface;
- temporal, provenance and critical-edge completeness checks;
- material contradiction as a resolution blocker and energy barrier;
- frontier-aware bounded recursive recovery;
- operational opposition `reopen_nodes` for targeted secondary research;
- governed statuses: `resolved`, `provisionally_resolved`, `contested`, `evidence_required`, `abstain`, `speculative`;
- no-silent-promotion enforcement;
- deterministic compact epistemic receipts for durable storage;
- persistent checksummed model-world ledger and audits;
- relation ontology and ambiguity-preserving entity resolution;
- embedded C++ API, C API, CLI and authenticated POSIX HTTP endpoint.

## Compact receipts instead of full-bundle persistence

The complete FiberBundle is normally an ephemeral query workspace. Persist a content-addressed receipt instead:

```cpp
#include "graphene/epistemic_receipt.hpp"

HypoKoshRuntimeResult result = runtime.reason(query, signature, options);
CompactEpistemicReceipt receipt =
    build_compact_epistemic_receipt(result);
```

The receipt retains selected path IDs, evidence/source/derivation lineage, bundle and evidence references, governed status, energy, semantic-verification state and residual uncertainty without copying source documents, indexes or every recursive-cycle state.

See [`docs/REASONING_MODES_AND_RECEIPTS.md`](docs/REASONING_MODES_AND_RECEIPTS.md).

## Build and test

For a first local build, keep the optional server disabled:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

### Validation status

The full pre-remediation split-source baseline passed **45/45 tests** with zero failures. The current frontier-aware changes have additionally passed:

- strict C++20 syntax validation with `-Wall -Wextra -Wpedantic -Werror` for the modified dialectic, runtime, runtime-contract and intervention benchmark translation units;
- a rebuilt 700-execution controlled intervention suite with all frozen gates passing;
- a standalone strict-build compact-receipt contract;
- a clean build, install, `find_package(GrapheneDB)` and external consumer execution for the package contract.

The final release head still requires an exact-head full run on an authenticated machine because the available GitHub-hosted jobs are terminating before checkout with zero recorded steps. No green hosted result is claimed.

## Controlled intervention result

Across six difficult evidence-recovery families:

| Policy | Final accuracy | Mean cycles | Mean visited states |
|---|---:|---:|---:|
| No cycle | 0.0% | 0.00 | 2.67 |
| Old unchanged-bundle stop | 66.7% | 1.83 | 10.17 |
| Frontier-aware targeted | 100.0% | 3.00 | 16.00 |
| Forced broad retrieval | 83.3% | 3.00 | 34.00 |

This is a controlled mechanism benchmark, not semantic truth or public-dataset end-to-end accuracy.

## CLI

```bash
./build/graphenedb_cli reason /tmp/graphenedb 16 <comma-vector> <signature> \
  --mode empirical --max-rounds 3 --json
```

The output includes governed status, bundle hashes, initial/final energy, final regime and certificate flags.

## HTTP

```bash
export GRAPHENEDB_API_KEY='development-key'
./build/graphenedb_server /tmp/graphenedb 16 8080 \
  --bind-address 127.0.0.1 --workers 2 --queue-capacity 32
```

Invoke `POST /v1/reason/runtime` with `X-API-Key`. The POSIX HTTP server remains unsupported on Windows; the embedded library is the preferred first-run path.

## Main implementation files

```text
include/graphene/fiber_bundle.hpp
include/graphene/path_verifier.hpp
include/graphene/epistemic_control.hpp
include/graphene/stability_critic.hpp
include/graphene/hypokosh_runtime.hpp
include/graphene/epistemic_receipt.hpp
src/fiber_bundle.cpp
src/epistemic_control.cpp
src/stability_critic.cpp
src/stability_critic_part_*.inc
src/dialectic.cpp
src/dialectic_frontier_part_*.inc
src/hypokosh_runtime.cpp
src/hypokosh_runtime_frontier_part_*.inc
src/epistemic_receipt.cpp
```

Large translation units use deterministic wrapper files plus `.inc` fragments so the complete source is reviewable and transportable without an unapplied patch.

## Honest limitations

- no global asymptotic-stability proof for an unbounded model world;
- no completed downloaded 2,500-record public-data benchmark yet;
- structural stability is not semantic truth;
- model world is a local bounded ledger, not a distributed autonomous scheduler;
- generic parsing is bounded and is not general natural-language understanding;
- clean final-head Windows validation remains pending;
- long-duration production-hardware soak, SBOM/security scan and signed release certification remain open gates.
