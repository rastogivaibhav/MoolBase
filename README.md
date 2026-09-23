# GrapheneDB v0.6.0-alpha.1

GrapheneDB is an experimental persistent epistemic reasoning substrate for agentic systems. It preserves evidence lineage, competing hypotheses, contradiction and bounded reopening so a system can show not only what it believes, but why the evidence process has or has not earned convergence. HypoKosh is the competing-hypothesis runtime; DWM is the challenge/reopen/synthesis loop.

## Maturity

**Experimental developer alpha for research and controlled pilots. Not enterprise GA and not a semantic truth engine.**

## Independent reproduction — start here for the epistemic proof

If you want to test the current GrapheneDB + HypoKosh + DWM thesis rather than install the older packaged alpha first, use a clean checkout of `master`:

```bash
git clone https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
python3 scripts/run_flagship_perturbations_v1.py
```

This one command rebuilds and replays the frozen flagship proof, verifies its canonical mechanism receipt, runs five pre-registered adversarial perturbations, and writes machine-readable receipts. It requires no hosted model or API key.

Canonical flagship mechanism receipt:

```text
36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef
```

See [Independent flagship reproduction](docs/INDEPENDENT_REPRODUCTION.md) for expected P1–P5 hashes, interpretation, claim boundaries and how to submit an external reproduction/critique. Public reproduction request: issue #38.

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


## License and verified distribution

GrapheneDB is distributed under the **Apache License 2.0**. The repository
includes the canonical license text in `LICENSE`, project attribution in
`NOTICE`, third-party distribution boundaries in
`THIRD_PARTY_NOTICES.md`, and DCO 1.1 contribution provenance.

Official distribution tooling rejects packages missing required legal files,
binds the release manifest to the exact source commit, emits SHA-256 checksums
and an SPDX 2.3 SBOM, and can generate GitHub/Sigstore provenance attestations.

See [Distribution security and license verification](docs/DISTRIBUTION_SECURITY.md).

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

Recent exact-head pull-request gates for the current flagship and perturbation work have completed successfully across the main CI, developer-experience, alpha-release, paper-system-conformance and dedicated flagship workflows. Those checks establish internal reproducibility at the stated mechanism boundaries; they do not establish independent validation or enterprise GA. See issue #25 for the current evidence and exit criteria.

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
