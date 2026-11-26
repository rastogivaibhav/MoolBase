# EGPM gate ledger — iteration 06 (before new inference results)

Scope: original MeMo MuSiQue selection; research branch only. Current development run [45905423044](https://github.com/rastogivaibhav/MoolBase/actions/runs/45905423044), code a49e7a1. At inspection one job model conversion running, seven queued. No duplicate run submitted.

| Gate | Status | Evidence / unresolved condition |
|---|---|---|
| G1 | PENDING | EGPM05 8/8 CI and eight fixture checks passed; new staged regression not yet verified. |
| G2 | PENDING | EGPM05 all 126 proposed triples rejected before verification. 86 valid bracketed IDs, 40 other invalid IDs, zero admitted edges across 16 graph arms. New provenance auditor unit fixtures passed locally; need authoritative corpus audit of real outputs and adversarial acceptance tests. |
| G3 | FAIL (EGPM05) | 0 independently verified proposals out of 126. EGPM06 staged extraction and oracle diagnostics running. |
| G4 | FAIL (EGPM05) | 0 complete paths in real QA, despite native/Python fixture parity. Need real native complete paths and durable provenance/receipts. |
| G5 | FAIL (development) | EGPM05 MeMo 2/8 EM, D text 1/8, D graph 0/8; no superiority. No statistical inference from tuned sample. |
| G6 | PENDING | Preregistered two disjoint 100-question QA splits and fixed statistical comparison before running QA. No unseen QA experiment yet. |

Prior full retrieval: BM25 recall@20 62.30%, native MiniLM 65.25%, hybrid 68.23%; native cosine parity 99.75%; retrieval only. [Full benchmark](https://github.com/rastogivaibhav/MoolBase/actions/runs/45883634716). EGPM05 [run](https://github.com/rastogivaibhav/MoolBase/actions/runs/45900548360), audited all 8 artifact IDs 15156207012, 15155284632, 15156621099, 15155218115, 15156281599, 15156256697, 15156037809, 15155467939. Offline split reproducibility, invalid population, missing results rejection, synthetic positive and null paired controls passed on actual 1000-ID retrieval artifact. Eight provenance-audit fixture cases passed locally: complete path, unknown source, altered text, offset tamper, missing edge audit, reversed path, weak entailment, accepted answer without path. No model evaluations in this iteration. No claim of native execution for the local tests. Commit uses skip-ci to avoid triggering duplicate ongoing matrix jobs. Next: inspect EGPM06 completed artifacts, debug any failures, audit against full corpus, implement independent relationship precision evaluation and freeze full inference configuration before heldout. No paid resources or production changes.
