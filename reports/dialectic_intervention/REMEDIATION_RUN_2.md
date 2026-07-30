# Dialectic intervention remediation — Run 2

## Changes under test

- recovery recursion is defect-specific and no longer runs for verifier-only or human-evidence tasks;
- opposition-only secondary research is opt-in through `RuntimeOptions::enable_opposition_research`;
- opposition `reopen_nodes` now seed downstream graph anchors in the next expansion cycle;
- completed-bundle equality no longer automatically means no progress;
- traversal frontier progress and bounded unchanged-bundle patience are tracked separately;
- Lyapunov cycle identity includes the bounded retrieval state, avoiding a false limit cycle when the same completed bundle is reached with a deeper frontier;
- runtime receipts expose frontier progress and no-progress termination.

## Controlled execution

The source-isolated intervention harness executed **700 deterministic runs** across seven topologies and five policies.

Hard-family aggregate:

| Policy | Final accuracy | Mean cycles | Mean visited states |
|---|---:|---:|---:|
| no cycle | 0.0% | 0.00 | 2.67 |
| current unchanged-bundle stop | 66.7% | 1.83 | 10.17 |
| frontier-aware targeted stop | 100.0% | 3.00 | 16.00 |
| forced targeted cycles | 100.0% | 3.00 | 16.00 |
| forced broad retrieval | 83.3% | 3.00 | 34.00 |

All frozen diagnostic gates passed. In particular, frontier-aware targeted recovery reached the correct root in the three- and four-hop cases, while broad retrieval still failed the noise trap.

## Validation performed

- modified `src/dialectic.cpp`: C++20 `-Wall -Wextra -Wpedantic -Werror -fsyntax-only` passed;
- modified `src/hypokosh_runtime.cpp`: C++20 `-Wall -Wextra -Wpedantic -Werror -fsyntax-only` passed against the current public header contract reconstructed over the source snapshot;
- modified runtime contract test: C++20 strict syntax validation passed;
- intervention benchmark compiled and executed against the exact unchanged storage/dialectic base used by Run 1.

## Claim boundary

This remediation closes the identified controller-level stopping defect in source and controlled ablation. The exact final GitHub head still needs a clean authenticated clone, full CMake build, CTest run and cross-platform execution. GitHub-hosted jobs have previously terminated before checkout, so a hosted green result is not claimed here.
