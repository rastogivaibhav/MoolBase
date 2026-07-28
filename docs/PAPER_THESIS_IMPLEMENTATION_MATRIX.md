# Paper Thesis Implementation Matrix

This file is the source-to-test contract for the TheHypoKosh experimental runtime. A capability is marked implemented only when executable source and at least one direct test are present in this repository.

| Thesis capability | Implementation | Direct validation | Status / limitation |
|---|---|---|---|
| Temporal facts, provenance, contradiction and supersession | `include/graphene/types.hpp`, `include/graphene/epistemic.hpp`, storage and dialectic sources | core, dialectic, hyperedge and P0 tests | Implemented; causal edges remain evidence-bearing claims rather than proof of causal identification. |
| Multi-path non-convergent expansion | `DialecticEngine::expand` and `CompleteHypoKoshRuntime` | `graphenedb_dialectic_tests`, runtime and generic-pipeline tests | Implemented with configured depth/path/branch budgets. |
| Immutable FiberBundle | `fiber_bundle.hpp/.cpp` | `graphenedb_hypokosh_runtime_tests`, adversarial critic tests | Implemented; exact duplicates collapse and independent support is source-lineage aware. |
| Lyapunov-style critic | `stability_critic.hpp/.cpp` | runtime and adversarial critic tests | Implemented as `StabilityCriticV0`; not a formally proven Lyapunov function. |
| Corrective escape | `escape.hpp/.cpp` | runtime and adversarial critic tests | Implemented as bounded plans; external discovery is not executed automatically. |
| Convergent compression | existing dialectic convergence plus complete runtime | dialectic and runtime tests | Implemented; original FiberBundle hash is retained and not mutated. |
| Opposition engine | existing `DialecticEngine::oppose` | dialectic, runtime and server contract tests | Implemented with bounded reopen decisions and deterministic findings. |
| Re-expansion and revised convergence | `CompleteHypoKoshRuntime` | runtime and server contract tests | Implemented with maximum rounds and resource budgets. |
| Governed answer projection and abstention | `hypokosh_runtime.hpp/.cpp` | runtime, generic pipeline, CLI and server tests | Implemented statuses: resolved, provisionally resolved, contested, evidence required, abstain and speculative. |
| No silent truth promotion | runtime projection and model-world mutation guards | runtime and adversarial tests | Implemented; reinforcement affects salience, not truth origin. |
| Recursive self-healing | `self_healing.hpp/.cpp` | runtime tests and server receipt test | Implemented as bounded plans and labelled proposals; no autonomous external actions. |
| Typed model world and feedback | `model_world.hpp/.cpp` | save/load, audit and scheduler tests | Implemented as a local persistent ledger; not yet a distributed million-node scheduler. |
| Reasoning modes | empirical, balanced and theoretical options across dialectic/runtime | dialectic/runtime tests | Implemented; modes alter thresholds and admissible evidence. |
| Relation ontology | `relation_ontology.hpp/.cpp` | generic reasoning pipeline tests | Implemented core registry and aliases; ontology breadth remains extensible. |
| Entity resolution | `entity_resolution.hpp/.cpp` | generic reasoning pipeline tests | Implemented deterministic ambiguity preservation; not a learned cross-corpus resolver. |
| Generic relation ingestion | `generic_relation.hpp/.cpp` | generic reasoning pipeline tests | TSV, pipe, RDF-like and token-tagged edge records supported; parser is deliberately bounded. |
| CLI execution | `graphenedb_cli reason` | `graphenedb_cli_reason_tests` | Implemented. |
| HTTP execution and receipts | `POST /v1/reason/runtime` | live server runtime contract | Implemented on the POSIX pilot server. |
| Windows stale-lock handling | existing platform abstraction; separate candidate fix not merged | existing core lock tests on Linux only | The Antigravity shared-read correction remains a separate candidate. Clean Windows review and execution are required before it is merged or claimed. |

## Release interpretation

The implementation is an **experimental runtime**, not enterprise GA. The complete Linux source has passed release, warnings-as-errors, ASAN/UBSAN focused tests, CLI tests, and live server contracts. Formal mathematical stability proof, official external benchmark comparison, long-duration production-hardware soak, and clean Windows validation remain separate gates.
