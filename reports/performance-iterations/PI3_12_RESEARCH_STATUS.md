# PI3.12 canonical research status

Date: 2026-08-19

This is a **status/evidence record only**. It does not mean the complete canonical PI3.12 source tree has been materialized into this GitHub branch. PR #20 remains a draft reconciliation PR and must not be merged as the complete implementation.

## Evidence-ranking result

PI3.12 starts from the accepted official-scope additive PI3.11 stack (`M -> M ∪ O -> M ∪ O ∪ S`) and ranks the resulting source-message evidence set for downstream reasoning.

Untouched conversations 5-9 (981 queries):
- full additive evidence reach: **83.2824%**
- mean full source-message candidates: **45.09**
- learned top-15 reach: **77.6758%**, retaining **93.27%** of full coverage
- learned top-30 reach: **80.9378%**, retaining **97.18%** of full coverage
- top-30 mean evidence items: **26.61**, about **40.98% fewer downstream items** than the full set
- learned top-50 reach: **82.7727%**, retaining **99.39%** of full coverage

The predeclared 10-15 item target required >=95% retention. It therefore **FAILS**.

Paired query bootstrap for top-15 vs full:
- delta: **-5.61 pp**
- 95% CI: **[-7.03 pp, -4.28 pp]**

The loss is not treated as noise.

## Safe output-budget decision

A train-only 10-vs-15 risk selector reaches **76.35%** held-out evidence reach with **91.68%** retention. A 15-vs-30 selector reaches **77.68%** with **93.27%** retention.

Decision: **current learned safe-output-budget/early-exit policy REJECTED for production.**

The top-30 point is accepted only as a useful research frontier. Its ~41% reduction is a downstream evidence/attention reduction, not a database-compute reduction, because full additive candidates are still generated upstream.

## Product / causal / architecture integrity

Fresh PI3.12 evidence:
- permanent PI3.12 gate: **PASS**
- product regression: **68/68 PASS**, 0 failures
- Microsoft CSuite: **15/15 correct causal roots**
- authoritative shadow mismatches: **0**
- architecture-preservation tests: **10/10 PASS**
- dialectic: PASS
- complete HypoKosh runtime: PASS
- frozen architecture fingerprint unchanged
- production `src/` / `include/` changes: **none**

## TheHypoKosh / Dialectical Model World revalidation

Fresh executable evidence strongly supports the papers' core architecture: typed provenance, FiberBundle multiplicity, contradiction preservation, no-evidence abstention, stability/Lyapunov control, bounded escape/self-healing, convergent/opposition/dialectic control, typed model-world state and no-silent-promotion governance are present and tested.

The papers are nevertheless stale as implementation-status documents:
- the current validated integrated GrapheneDB reasoning substrate is C++, while TheHypoKosh v0.1 describes a Rust-first package;
- the Dialectical Model World runbook describes Python modules and old test-count/proxy-baseline snapshots;
- those implementation sections should be revised as historical/prototype lineage.

Still unproven / future research includes:
- broad explanatory-answer superiority against official external RAG/GraphRAG/Self-RAG/ReAct/agent-memory baselines;
- autonomous causal discovery from raw text and real-world falsification;
- novel hypothesis invention rather than controlled hypothesis representation/escape;
- the proposed million-node dialectical/model-world workload;
- dedicated ~1B model integration and Kosh-aware model training.

## Sealed local PI3.12

Canonical local commit:
`762d5d3f013caf6fcba975e6a62150fdf082b425`

SHA-256:
- source ZIP: `f5b3985571a9df8f333466ece25a9d8de7917564bdc1f1bc8b1bdd6dac7cb3ef`
- evidence ZIP: `3847f5d930b4ab52470e5fcfba7c3f033849d8f46b074e6a27a33061c21e1960`
- PI3.11 -> PI3.12 patch: `e331161686b00e127392d9bbbdcc2ddb8767f46acfd96a9be923fea9432ae811`

## Final decision

- PI3.12 evidence-ranking research: **ACCEPTED**
- 10-15 evidence-item production target: **REJECTED**
- learned safe output budget: **REJECTED**
- top-30 frontier: **RESEARCH-ONLY**
- production retrieval/reasoning authority: **UNCHANGED**
- TheHypoKosh/Dialectical Model World core architecture: **substantially implementation-backed**
- papers' current implementation-status sections: **REQUIRE REVISION**
