# GrapheneDB / HypoKosh / DWM implementation lineage

Status: **public implementation-lineage note**

This document prevents architectural history from being mistaken for the current reference implementation.

## Current authoritative implementation

The current reference implementation is **GrapheneDB C++20**.

GrapheneDB is the public product and authoritative durable engine. The integrated reasoning stack is:

- **GrapheneDB** — persistent evidence/causal-memory substrate, lineage-aware FiberBundles, admissibility, receipts and durable storage;
- **HypoKosh** — competing-hypothesis/proposal runtime implemented through the current GrapheneDB C++ reasoning path;
- **DWM / bounded dialectic** — challenge, opposition, reopen and synthesis loop implemented through the current C++ dialectic/runtime path.

The authoritative build maps these mechanisms into the `graphenedb` target in `CMakeLists.txt`, including:

```text
src/entity_resolution.cpp
src/epistemic_receipt.cpp
src/epistemic_control.cpp
src/escape.cpp
src/fiber_bundle.cpp
src/generic_relation.cpp
src/hypokosh_runtime.cpp
src/model_world.cpp
src/path_verifier.cpp
src/relation_ontology.cpp
src/self_healing.cpp
src/stability_critic.cpp
```

The current paper-to-system gate is `docs/lab/PAPER_SYSTEM_CONFORMANCE_V1.md` and its executable runner is `scripts/run_paper_conformance_v1.py`.

PR #28 established a clean-checkout conformance receipt for the current paper boundary. It does not turn future-work concepts into implemented claims.

## Historical architecture lineage

### TheHypoKosh v0.1

TheHypoKosh introduced the competing-hypothesis, FiberBundle, stability/escape and self-healing architecture. Its historical implementation description was **Rust-first / Cargo-oriented**.

That description is prototype lineage. It is **not** the current GrapheneDB implementation language or package boundary.

The original legacy paper/package is not materialized as an authoritative current file on `master`. Repository evidence recording this transition is preserved in:

- `reports/performance-iterations/PI3_12_RESEARCH_STATUS.md`;
- `paper/README.md`;
- historical branch `release/hypokosh-thesis-v2-validated`.

Do not cite the historical Rust package description as the current reference implementation.

### Dialectical Model World

The Dialectical Model World work introduced the challenge/reopen/synthesis architecture and the broader model-world feedback direction. Historical runbook material described **Python modules** and older test/proxy-baseline snapshots.

Those Python/runbook descriptions are prototype lineage. They are **not** the current reference implementation.

Current bounded dialectic behaviour is documented in `docs/DIALECTIC_REASONING_V0.md` and implemented in the C++ GrapheneDB runtime.

Relevant historical repository lineages include:

- branch `agent/dialectic-model-world-v0`;
- the implementation-status correction preserved in `reports/performance-iterations/PI3_12_RESEARCH_STATUS.md`.

The current bounded DWM contract explicitly remains read-only with respect to durable model-world belief writes.

### Current GrapheneDB paper

The current paper package under `paper/` intentionally narrows earlier whitepaper claims to mechanisms supported by the integrated C++20 implementation and reproducible evidence.

Its relationship to earlier work is:

```text
TheHypoKosh architecture
        +
Dialectical Model World architecture
        ↓
integrated GrapheneDB C++20 reasoning substrate
        ↓
current GrapheneDB paper claim boundary
```

The current paper is not evidence that every earlier architectural ambition has been implemented.

## Current implementation map

| Concept | Current authority | Current evidence boundary |
|---|---|---|
| Persistent evidence/causal state | GrapheneDB C++ storage/runtime | embedded C++ implementation and release tests |
| Lineage-aware FiberBundle | `fiber_bundle` | source/evidence-family/derivation lineage and immutable bundle tests |
| Epistemic admissibility / contradiction blocking | `epistemic_control` + stability critic | current conformance T1/T2 |
| HypoKosh proposal/runtime path | `hypokosh_runtime` + governed learning | proposals remain hypothetical/read-only; no silent truth promotion |
| DWM challenge/reopen/synthesis | dialectic runtime | bounded operational reopen; no durable model-world belief write |
| Defect-specific recovery | escape + HypoKosh runtime | bounded/frontier-aware current-paper mechanism |
| Deterministic reasoning receipt | `epistemic_receipt` + runtime receipt | deterministic mapped fixtures |
| Governed policy promotion/rollback | governed learning | retrieval-policy state only, not durable DWM belief promotion |

## Historical results and corrections

Historical measurements, negative findings and retractions must remain visible.

In particular, this lineage note does **not** overwrite:

- PI3-series accepted/rejected research decisions;
- historical LoCoMo corrections/retractions;
- prior prototype test counts;
- frozen benchmark results;
- negative findings from earlier runs.

When a historical artifact reports a result from a Rust or Python prototype, report it as a historical prototype result unless the same claim has a current C++ reproduction receipt.

## Claims that remain future work

The implementation lineage must not be used to imply that the following are currently proven:

- autonomous causal discovery from raw text;
- autonomous external experiment execution;
- durable model-world belief promotion/revision across later world updates;
- hidden evidence-dependence discovery without configured lineage;
- semantic truth or general answer correctness;
- general superiority over RAG, GraphRAG, ReAct, agent-memory systems, Jev or other external systems;
- million-node dialectical/model-world production readiness;
- dedicated small-model/Kosh-aware training.

These require separate implementation and evidence.

## Publication rule

For current technical writing, demos and external evaluation:

1. Say **GrapheneDB** when referring to the product/runtime.
2. Describe **HypoKosh** as the competing-hypothesis/proposal runtime within the current GrapheneDB implementation.
3. Describe **DWM** as the bounded challenge/reopen/synthesis loop.
4. Label Rust/Python descriptions as historical prototype lineage.
5. Link current implementation claims to C++ source/tests and the paper-system conformance receipt.
6. Preserve historical negative results and corrections rather than silently replacing them.
7. Do not convert a future-work architecture concept into a current implementation claim.

## Provenance of this correction

The need for this lineage correction was explicitly recorded in `reports/performance-iterations/PI3_12_RESEARCH_STATUS.md` on 2026-08-19:

- current validated integrated GrapheneDB reasoning substrate: C++;
- historical TheHypoKosh implementation description: Rust-first;
- historical DWM runbook description: Python modules and old snapshots;
- core architecture: substantially implementation-backed;
- implementation-status sections: require revision.

Issue #29 tracks this correction. The purpose of this document is to make the authoritative public interpretation durable without rewriting the historical artifacts.
