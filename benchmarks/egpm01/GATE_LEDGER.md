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

## EGPM07 completed / EGPM08 recovery
[EGPM07 run](https://github.com/rastogivaibhav/MoolBase/actions/runs/45908047493), 956386f, 8/8 successful jobs. Logs show zero supported links and zero complete paths across all 40 graph arms; MeMo 2/8 EM, oracle text 4/8, graph 0/8. Candidate generation often returned no admissible answer even with oracle documents; remaining proposals failed anchor, endpoint or semantic checks. G1 PASS for EGPM07 development only; G2 PENDING (no accepted real answers); G3 FAIL; G4 FAIL; G5 FAIL development; G6 PENDING. EGPM08 isolates candidate starvation by adding a single-answer proposal with the same source context, retaining strict grounding, logging raw list and single responses and preserving previous round diagnostics. No change to semantic thresholds, labels or frozen split. New smoke assertion rejects abstention as candidate. This is a development adaptation, not matched-compute evidence. Artifact links available on run page; logs reviewed, full artifact audit pending. Next inspect candidate yield and rejection causes; no held-out run.
