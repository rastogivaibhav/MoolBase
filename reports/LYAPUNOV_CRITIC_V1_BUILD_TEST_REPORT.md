# Complete Discrete Lyapunov Critic v1 — Build and Test Report

Date: 28 July 2026

## Implemented

- seven-dimensional bounded epistemic error state;
- mode-aware goal sets;
- positive weighted quadratic Lyapunov candidate;
- computed quadratic lower and upper bounds;
- per-cycle energy and drift;
- sufficient-decrease condition;
- contraction ratios;
- equilibrium, descent, marginal, divergence, oscillation, and limit-cycle regimes;
- finite-trajectory practical-stability and convergence certificates;
- immutable bundle-hash cycle detection;
- integration with bounded HypoKosh re-expansion and governed projection;
- CLI and authenticated HTTP output;
- model-world recording of final energy and regime;
- compatibility facade for `StabilityCriticV0` callers.

## Full release regression

```bash
cmake -S . -B build_lyapunov \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build_lyapunov -j2
ctest --test-dir build_lyapunov --output-on-failure
```

Result:

```text
45/45 tests passed
0 failures
23.65 seconds (exact split-source integration run)
```

Coverage included storage, C API, WAL/crash/fault handling, stale locks, lattice storage and traversal, one-million-record smoke, extraction ingestion, dialectic reasoning, hyperedges, governed learning, generic relation reasoning, CLI reasoning, the complete runtime, adversarial stability cases, the new Lyapunov suite, OpenAPI checks, and live server contracts.

## New critic tests

The direct Lyapunov tests validate:

- zero energy at the configured goal-set origin;
- strictly positive energy away from the goal set;
- positive quadratic bounds;
- monotonic descent and equilibrium dwell;
- deterministic replay;
- energy increase and divergence;
- oscillation;
- repeated-state limit cycle;
- invalid negative-weight rejection;
- mode-sensitive empirical strictness.

## HTTP contract

Observed result:

```json
{"evidence_edges":4,"final_bundle_hash":13384482197379671592,"hypokosh_runtime_contract":true,"initial_bundle_hash":13384482197379671592,"lyapunov_final_energy":0,"lyapunov_regime":"equilibrium","status":"provisionally_resolved"}
```

The result remained provisionally resolved because the governed answer layer also considers opposition and residual uncertainty; Lyapunov equilibrium alone does not promote a claim to truth.

## Compiler warnings

The newly changed critic, runtime, escape/self-healing, and direct tests passed:

```text
-Wall -Wextra -Wpedantic -Werror -fsyntax-only
```

A whole-library warnings-as-errors configuration exposed an unrelated pre-existing unused helper in `src/learning.cpp`; that pre-existing warning was not silently represented as a new-critic failure.

## Sanitizers

Standalone ASAN/UBSAN execution passed for:

- complete Lyapunov critic tests;
- adversarial stability tests;
- complete HypoKosh runtime integration linked against the release engine.

No AddressSanitizer or UndefinedBehaviourSanitizer violation was reported.

## Clean package reproduction

The final source ZIP was extracted into a new directory with no previous CMake cache or build output. The extracted source configured and built successfully. All **45/45 tests passed with 0 failures**. Because the container command reached its execution-time ceiling after test 37, tests 38–45 were resumed in a second bounded CTest invocation; each remaining test passed.

## Formal limitation

The candidate is positive definite in the configured error coordinates and the runtime certifies the observed finite trajectory. This is not a proof of global asymptotic stability for all possible model-world transitions or unbounded external discovery actions.
