# MoolBase v0.6.0-alpha.2 — developer preview

MoolBase stores evidence, competing explanations and inspectable decision receipts for applications working with changing or contradictory information.

This developer preview includes the embedded C++20 database, optional POSIX HTTP server, dependency-free Python examples and an actual-engine WebAssembly Evidence Lab. Native API/package names remain GrapheneDB for compatibility.


## Architecture in this preview

- **MoolBase Core** — durable evidence/provenance and causal-memory storage with WAL/checkpoint persistence.
- **Dense graphene-inspired hexagonal lattice topology** — durable axial `q/r/layer` coordinates, deterministic hex-spiral placement, validated bonds and optional lattice-aware retrieval. This is a database topology/retrieval primitive, not a material-science simulator.
- **FiberBundle** — deterministic, hashable evidence-path projection preserving target, role, lineage and independent-support structure.
- **HypoKosh / Hypothesis Engine** — competing hypotheses with governed convergence or abstention.
- **DWM / Dialectic Engine** — material opposition, challenge, bounded reopen/re-expansion and governed revision.
- **Epistemic receipts** — compact machine-readable provenance for the resulting governed state and transitions.

These layers are composable; applications do not have to adopt the complete reasoning stack.

## What ships

- Atomic evidence retirement, audit and replacement in one existing-format WAL transaction.
- Bounded customer examples for incident investigation, account-memory correction and conflicting reports.
- Duplicate flooding and late contradiction coverage; lifecycle operations at the 100-active limit, with 200 total history events.
- Strict worker inputs and normal CLI parse diagnostics.
- Python lifecycle and HTTP ingestion/runtime examples, including evidence replay after server process restart.
- Browser desktop/mobile verification, receipts, provenance documentation and runnable sample source.

## Install and try

Use the versioned source archive on this release, or clone the tag:

```bash
git clone --branch v0.6.0-alpha.2 --single-branch https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_SERVER=ON
cmake --build build-showcase --target moolbase_customer_showcase graphenedb_server -j2
python3 examples/customer_showcase/python_memory.py
python3 examples/customer_showcase/python_http.py
```

The HTTP server requires POSIX. Windows supports the native library and memory example; configure with `-DGRAPHENEDB_BUILD_SERVER=OFF` and build the adapter target only. Native CMake integration uses `find_package(GrapheneDB CONFIG REQUIRED)` and `GrapheneDB::graphenedb`.

## Verification and integrity

The release workflow verifies native Release builds and installed package consumers on Linux, macOS and Windows, runs LLVM WAL fuzz smoke, and verifies real Chromium desktop/mobile flows. It publishes only after these checks succeed. Source/sample distributions contain Apache-2.0, project and third-party notices, exact source-commit manifests, SHA-256 checksums and SPDX 2.3 inventories. Linux native packages are checked by the existing distribution policy validator. Build and SBOM attestations use the repository distribution workflow where available; do not infer an attestation from a checksum alone.

## Boundaries

Developer alpha for evaluation and controlled pilots. No enterprise GA, semantic truth or production-scale superiority claim. Applications supply vectors/provenance/verification; fixed vectors and realistic synthetic fixtures demonstrate mechanisms. Support is a policy score, not calibrated probability.

Browser files are session-local and refresh discards them. Native evidence persists, but the adapter's prior reasoning state stays in memory. HTTP requests currently do not expose the complete native supersession lifecycle or persist prior reasoning state between requests. No published MCP server is included. Use the agent integration guide to choose the correct surface.

The release branch is separate from frozen research commits. Release preparation does not change scoring manifests or trigger a new scientific experiment.
