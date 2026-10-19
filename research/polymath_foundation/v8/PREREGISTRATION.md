# MOOL-POLYMATH-F8-001 — Semantic parsing, bounded mechanism selection and epistemic opposition

**Protocol preregistered 19 October 2026 before any F8 model run or scores.** Base: frozen successful F7 commit `420268f853bd5603c8a2d25cca7958ad9657bfd8`. No changes to F1–F7 scoring protocols/results.

## Research objective

Measure whether a real language model can translate unfamiliar natural-language observations into **typed, unit-bearing mechanism inputs**, after which the existing **real C++ MoolBase selector** chooses among the three original unverified procedures, the original v6 engine checks assumptions and executes, and the score is computed from separately held original v3 oracle logic.

Prior 108/108 native routing success was **not** learned polymath understanding: field names and output targets were virtually unique. F8 removes canonical variable, target and mechanism names from candidate natural-language prompts. All toy scientific knowledge and language templates are project-authored; the question generator is public, **not independently blind** and not a scientific evaluation of generalization.

## Design and frozen cases

24 fixed questions in ordered/shuffled categories at seed **20261021**:
- 12 admissible (4 each networks, biology, supply chain);
- 6 actual assumption violations (2 each);
- 3 unrelated scientific questions (optics, astronomy, acoustics);
- 3 deliberately mixed-domain prompts containing two legitimate complete input sets and ambiguous combined instructions.

Input to each parser arm is **only an opaque ID and full natural language question**. Canonical mechanism ID, domain, formula ID, typed variables, target name, target unit, normalized values and evaluation class are concealed in the *candidate request*, but the public repo generator itself is visible, so contamination remains possible.

Use the original F3 fixed mathematical task generator at seed 20261021, select first four valid and first two invalid per domain. Vary semantic lexical names, output paraphrases, and units in prose (packet slots, items/day, dimensionless gain, control units). Gold for category/domain/normalized input/result remains in a separate JSON evaluation file and is not loaded by the parser or native C++ selector. Numeric magnitudes are deterministically regenerated and need not be all unique.

## Frozen arms and mechanics

- **B0 — exact-schema no semantic parser:** the existing F7 schema-only baseline receives an empty/unrecognized normalized representation for each natural-language question. It cannot trivially exploit canonical variable names and will abstain. This is a lower-bound control, not a strong alternative.
- **B1 — authored-alias deterministic parser:** static dictionary + strict template parsing recognizes only explicitly curated terms in the question. It feeds the same exact F7 schema-only selector and **unchanged C++ v6 executor**. No model or hidden gold.
- **B2 — actual Qwen2.5-1.5B-Instruct Q4_K_M:** model emits ONE structured normalized question object containing `description`, `target`, `target_unit`, `inputs` (names, typed values and units). Model may also abstain with explicit rationale. Use a stateless inference call for each of the 24 questions. Model output is never repaired using gold or oracle. Malformed/no output/unsupported units/duplicate fields count as failures.
- **B3 — same B2 outputs through actual native MoolBase:** feed each B2 normalized question to **GrapheneDB native v7 selector** after persistence/restart and evaluate with the same unchanged C++ v6 executor. B2 uses the non-native B1 schema selector. **B3 is not an additional model arm**; it checks native integration equivalence, not an independent intelligent improvement. If both selectors use the same policy, B2 and B3 SHOULD tie.

System gets mechanism cards and normalized schema definitions in the system prompt (identical training information for every question), no gold answers or oracle logic. Pin model SHA `91cad51170dc346986eccefdc2dd33a9da36ead9` and llama.cpp SHA `ff5a23376725e5a889fa0bfb987b7541dc304da0`; CPU-only; temperature=0; seed=20261021; max output 240 tokens and context window 4096. Record model file hash, actual raw completions, latency and usage, prompt bytes, parser failures, exact GitHub commit and all native receipts.

## Opposition and DWM research boundary

The original Dialectical Model World design distinguishes non-convergent alternatives, convergent compression, opposition, re-expansion and **external outcome feedback**. Avoid passing a single DWM `evidence_required` status as if this completed that cycle. F8 uses a **strict policy opposition gate** that challenges proposed normalized inputs: duplicate entries, schema conflict, insufficient variables, inapplicable units, mixing domains or out-of-scope questions must result in refusal; no challenge changes numerical values, promotes facts to Verified or fabricates external evidence. This gate is ordinary deterministic policy code, **not the real HypoKosh–DWM engine**; explicitly mark full dialectical cycle **NOT_RUN**. The real runtime was separately exercised in F5.

## Required metrics and integrity

For each arm: exact semantic normalization accuracy, correct accepted mechanism, end-to-end correct number or justified abstention, per-category performance, tool numeric coverage, unsafe acceptance, over-abstention, malformed-model response rate and native/non-native parity. OOD/composite tasks should be refused, **not** interpreted as unbounded analogies.

The gold scorer uses the unchanged F3 original oracle only **after** all normalized records and native execution receipts are frozen. Parsing should be scored wrong for incorrectly normalized valid questions even if accidentally yielding the same number; all 24 trial identities must be present. Keep negative results; no post-outcome task/model/threshold tuning.

## Scientific limitations and next gate

This pilot uses authored paraphrase templates tied to known source cards; not independent expert-led, true transfer, learning in weights, free analogy discovery, autonomous research or validated causal reasoning. Scored model performance on 24 examples is underpowered and potentially brittle. B0 is intentionally weak and B1 dictionary handcrafted; compare B2 on exactly the same questions and capability limits, but do not claim superior scientific ability.

To test the paper's real polymath hypothesis afterward, separately recruit expert-authored, untouched evaluation data; compare a truly strong language model with equivalent tools/budgets and actual multi-hypothesis HypoKosh–DWM opposition/feedback state with source authentication, independent empirical outcomes, alternative models and no false truth promotion. Until then, claim only engineering feasibility.
