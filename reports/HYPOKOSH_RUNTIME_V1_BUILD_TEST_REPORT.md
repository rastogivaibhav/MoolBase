# TheHypoKosh Experimental Runtime v1 — Build and Test Report

Date: 28 July 2026

## Scope

This run validates the materialised source implementation of:

- immutable FiberBundle
- StabilityCriticV0
- bounded CorrectiveEscape
- convergence, opposition and bounded re-expansion
- governed answer projection and no-silent-promotion checks
- recursive self-healing plans
- persistent model-world ledger and scheduler
- relation ontology, entity resolution and generic relation ingestion
- CLI and authenticated HTTP runtime execution

## Release build

```bash
cmake -S . -B build_server_v1 \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build_server_v1 -j2
ctest --test-dir build_server_v1 --output-on-failure
```

Final integrated-source result:

- 44 tests discovered
- 44 tests passed
- 0 failures
- CTest wall time: 21.13 seconds (final exact-source rerun)

The suite includes storage/crash/fault tests, lattice tests, CLI reasoning, dialectic, generic reasoning, complete runtime, adversarial stability tests, hyperedges, governed learning, OpenAPI validation and live server contracts.

## Warnings-as-errors

The new runtime, stability, generic-ingestion and test targets were compiled with:

```text
-Wall -Wextra -Wpedantic -Werror
```

Result: build and focused tests passed.

## Sanitizers

Focused runtime targets were built and executed with AddressSanitizer and UndefinedBehaviourSanitizer.

Validated targets:

- complete HypoKosh runtime
- StabilityCriticV0 adversarial tests
- generic reasoning pipeline
- CLI reason contract

Result: all passed with no reported sanitizer violation.

## HTTP contract

The live contract test created multiple independent evidence paths and an alternative root, invoked `POST /v1/reason/runtime`, and verified:

- Graphene expansion executed
- FiberBundle created
- StabilityCriticV0 executed
- corrective escape evaluated
- convergence and opposition executed
- governed projection executed
- model-world feedback executed
- evidence edges returned
- deterministic initial/final bundle hashes
- reasoning did not mutate the database

Observed contract result:

```json
{
  "evidence_edges": 4,
  "final_bundle_hash": 13384482197379671592,
  "hypokosh_runtime_contract": true,
  "initial_bundle_hash": 13384482197379671592,
  "status": "provisionally_resolved"
}
```

The provisional status is intentional: the governance layer retained residual uncertainty rather than forcing a resolved answer.

## Non-claims and remaining gates

- `StabilityCriticV0` is not a formal Lyapunov proof.
- No official external benchmark result is claimed by this report.
- The model world is a local bounded ledger, not a distributed autonomous million-node scheduler.
- The generic parser is intentionally bounded and is not a general natural-language semantic parser.
- The separate Antigravity Windows stale-lock candidate is not included in this runtime commit; it still requires review and clean Windows validation.
- Production GA still requires external benchmarks, platform matrix evidence, long-duration soak and security/release gates.

## Clean packaged-source reproduction

The final source package was extracted into a fresh directory with no prior build cache. The clean reproduction configured, built, and passed 44/44 tests with 0 failures in 20.65 seconds.
