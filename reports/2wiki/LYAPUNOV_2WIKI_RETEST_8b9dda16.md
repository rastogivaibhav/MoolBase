# FiberBundle v2 / Lyapunov Critic Retest

## Scope

This report records a source-isolated replay of the current FiberBundle v2 and Lyapunov critic logic after the final remediation changes on draft PR #6.

The input was the preserved deterministic 1,000-example `2wiki_sample.tsv` artifact produced by the last successful 2Wiki workflow run (`30410428303`). The sample was reused unchanged; it was not replaced with generated examples.

The replay compiled the current FiberBundle and Lyapunov algorithms as C++20 with optimisation and compiler warnings enabled, then applied the same evidence-corruption conditions used by `bench/bench_2wiki_lyapunov.cpp`.

This is not a substitute for the repository's complete CMake, sanitizer, fuzz, packaging, or cross-platform GitHub Actions matrix.

## Shortcomings fixed before the retest

- Source/document ancestry now takes precedence over per-edge evidence identifiers.
- Semantic verification and independent corroboration are scoped to the selected target.
- Independently supported alternative targets remain visible as opposition.
- Shared source ancestry correlates paths even when callers provide different family labels.
- Exact duplicate paths are merged conservatively and deterministically.
- Conflicting duplicate verifier results cannot retain the optimistic result through input ordering.
- The 2Wiki harness now declares the actual final path node as its anchor.
- Controlled missing-hop corruption uses the canonical `MISSING_EVIDENCE` finding code.

## Preserved 1,000-example sample result

| Diagnostic | Result | Acceptance target |
|---|---:|---:|
| Missing-hop energy above gold | 100.0% | >=90% |
| Contradiction energy above gold | 100.0% | >=90% |
| Same-source duplicate not rewarded | 100.0% | >=98% |
| Irrelevant distractor not rewarded | 100.0% | >=95% |
| Repair trajectory monotonic | 100.0% | >=90% |
| Duplicate independence inflation | 0.0% | 0% |
| Duplicate pattern-lock reduction | 0.0% | 0% |
| Distractor pattern-lock reduction | 0.0% | 0% |
| Wrong-complete dynamics separation | 0.0% | informational boundary |

Mean Lyapunov energy:

| Condition | Mean energy |
|---|---:|
| Gold | 0.090251 |
| Missing hop | 1.000000 |
| Contradiction | 0.850000 |
| Same-source duplicate | 0.090251 |
| Irrelevant distractor | 0.315789 |

## Boundary interpretation

Gold stability, repair-goal and repair-convergence rates were 0% in this diagnostic. That is not an acceptance failure. The deterministic 2Wiki records normally provide one canonical evidence chain, not independent corroborating routes. The critic therefore permits monotonic repair while refusing to manufacture equilibrium from a single chain.

The structurally complete wrong condition has the same Lyapunov energy as gold. This is intentional: the Lyapunov critic measures reasoning stability and evidence structure, not semantic truth. Final resolution remains controlled by the separate semantic-verification and epistemic-admissibility gates.

## Direct critic contract

The direct Lyapunov contract also passed in an isolated C++20 build, including:

- zero energy at the configured equilibrium;
- positive, bounded quadratic state;
- descending trajectory and equilibrium dwell;
- divergence detection;
- oscillation detection;
- limit-cycle detection;
- invalid-weight rejection;
- empirical-mode provenance sensitivity;
- deterministic repeated evaluation.

## CI status

The latest GitHub Actions attempts for the branch failed before any workflow step was created or executed. The job records contain no checkout, compile, test, or artifact steps. Consequently this report does not claim a successful latest-head GitHub Actions run or multi-platform release readiness.

The pull request must remain draft until the complete Linux, macOS, Windows, sanitizer, fuzz, packaging, release-smoke and locked benchmark workflows execute successfully from the final head.
