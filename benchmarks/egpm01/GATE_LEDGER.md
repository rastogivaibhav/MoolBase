# EGPM gate ledger — iteration 07

Current branch work follows [EGPM06](https://github.com/rastogivaibhav/MoolBase/actions/runs/45905423044), a49e7a1. Preregistration b937c0d unchanged. [EGPM03 full retrieval](https://github.com/rastogivaibhav/MoolBase/actions/runs/45883634716): 1000 QA selection, 5296 docs, native semantic recall@20 65.25%, BM25 62.30%, hybrid 68.23%; parity 99.75%.

| Gate | Status | Evidence |
|---|---|---|
| G1 | PASS for EGPM06, PENDING for EGPM07 | 8/8 staged jobs and native regression tests passed; new tests submitted. |
| G2 | PENDING | EGPM07 source audit on actual corpus and negative fixture cases; external validation of accepted answers pending. |
| G3 | FAIL (development) | EGPM06 21 admitted edges, eight distinct claim/source/verdict combinations; 11 clearly supported, three ambiguous, seven unsupported as labelled in manual review. See EGPM07.md. Independent blinded precision evaluation pending. |
| G4 | FAIL (development) | 0 complete native paths on 8 questions, including oracle; candidate-directed proof pipeline implemented. |
| G5 | FAIL (development) | MeMo 2/8 EM, graph 0/8, oracle text 4/8. No superiority. |
| G6 | PENDING | Two disjoint 100-question split definitions frozen; no held-out QA run. |

Next: inspect EGPM07 CI and per-question artifacts, check actual candidate paths, wrong-answer rate, source provenance and verifier disagreements. Do not launch held-out until development implementation and all prerequisite gates are satisfied. No production changes or paid resources.
