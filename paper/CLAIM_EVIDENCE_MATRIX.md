# GrapheneDB paper claim-to-evidence matrix

This matrix is the release gate for every substantive claim in `paper/main.tex`. A claim must remain inside the strongest evidence class listed here.

## Evidence classes

- **I — Implemented:** present in the merged source tree.
- **L — Locally validated:** executed in a controlled local environment with preserved output.
- **R — Reproducible from repository:** source, command and expected gate are committed.
- **E — External/independent validation:** reproduced by an independent developer or external environment.
- **H — Hypothesis/future work:** not established by current evidence.

| Claim | Class | Repository evidence | Permitted wording | Prohibited wording |
|---|---|---|---|---|
| GrapheneDB is an embedded C++20 evidence and causal-memory database | I | `CMakeLists.txt`, `include/graphene`, `src`, `README.md` | “implemented embedded C++20 database” | “production-proven database” |
| FiberBundle separates graph route, source, evidence-family and derivation lineage | I/L | `include/graphene/fiber_bundle.hpp`, `src/fiber_bundle.cpp`, lineage tests | “represents and tests distinct lineage identities” | “guarantees real-world source independence” |
| Correlated paths do not count as independent corroboration | I/L/R | FiberBundle v2 tests and cross-dataset structural suite | “prevents configured shared ancestry from increasing independent-support count” | “detects all hidden dependence automatically” |
| Material contradiction blocks final resolution | I/L/R | epistemic controller, critic tests, FEVER/refutation structural gates | “implemented as a resolution blocker” | “resolves every contradiction correctly” |
| Recursive search is conditional rather than always-on | I/L | runtime options and controller logic | “re-expands for diagnosed graph-searchable defects” | “always finds missing evidence” |
| Opposition research uses reopen targets | I/L | runtime and dialectic implementation/tests | “reopen nodes influence targeted subsequent expansion when enabled” | “opposition independently discovers truth” |
| Previous completed-bundle stop failed on deeper chains | L/R | intervention benchmark and Run 2 report | “failed controlled three- and four-hop families” | “fails on 33.3% of all real-world reasoning” |
| Frontier-aware targeted recovery reached 100% on hard controlled families | L/R | `bench/bench_dialectic_intervention*`, `reports/dialectic_intervention` | “100% in the controlled hard-family suite” | “100% reasoning accuracy” |
| Broad forced retrieval reached 83.3% and averaged 34 visited states | L/R | same benchmark | report exact controlled result | claim general performance superiority |
| Targeted recovery averaged 16 visited states | L/R | same benchmark | report exact controlled result | infer production latency without measurement |
| Structural cross-dataset gates passed | L/R | `benchmarks/cross_dataset`, committed reports | “structural critic/control properties passed” | “HotpotQA/FEVER answer accuracy passed” |
| Lyapunov critic measures finite-trajectory practical stability | I/L | critic source and tests | “Lyapunov-inspired finite-trajectory diagnostic” | “formal global convergence proof” |
| Compact receipts avoid mandatory full-workspace persistence | I/L | `epistemic_receipt` implementation and contract test | “supports deterministic compact durable receipts” | “proves optimal compression” |
| GrapheneDB outperforms Neo4j, Qdrant, Milvus or external agents | H | no official head-to-head benchmark | “not evaluated; complementary positioning” | any superiority claim |
| GrapheneDB improves semantic answer correctness | H | no end-to-end semantic benchmark | “open research question” | “improves truthfulness/accuracy generally” |
| Million-node production readiness | H | no final rich one-million-node proof for this release | “future scale gate” | “million-node validated” |
| Enterprise security or GA readiness | H | security checklist remains open | “experimental alpha” | “enterprise-ready”, “secure by default” |

## Mandatory claim boundaries

The abstract, results and conclusion must include all of the following boundaries:

1. The intervention study is controlled and topology-preserving.
2. The cross-dataset suite evaluates structural evidence behaviour, not semantic answer quality.
3. Causal edges are reasoning assertions, not proof of causal identification.
4. The critic does not prove global asymptotic convergence.
5. No database-scale or external-agent superiority claim is made.
6. The system is not a semantic truth engine or production decision authority.

## Claims removed from the earlier whitepapers

The arXiv manuscript must not carry forward these earlier statements as established results:

- Rust-first implementation: the submitted GrapheneDB implementation is C++20.
- AGI-relevant or cognitive-substrate claims as a demonstrated outcome.
- One-million-object model-world capability as current evidence.
- Official superiority over RAG, GraphRAG, Self-RAG, ReAct or agent-memory systems.
- Recursive self-improvement from implementation outcomes.
- External discovery or autonomous scientific reasoning.

These may appear only as clearly labelled future research directions, and are omitted from the present core paper to preserve a narrow, falsifiable contribution.
