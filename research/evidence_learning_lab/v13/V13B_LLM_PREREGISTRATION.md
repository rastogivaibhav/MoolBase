# MOOL-EVIDENCE-LEARNING-V13B-001 — Matched-model LLM-only vs ordinary event retrieval

**Separate preregistration, 19 October 2026, before any V13B code, first model call or score.** V13 original SQL sandbox/validator and preregistration remain unchanged; new V13B evidence has its own ID/CI job.

## Question
Does ordinary timestamped event retrieval help an actually executed language model diagnose six internally controlled ERP/WMS fault injections, and how do both model conditions compare to the v13 transparent event-rule and actual native G1/G2 outcomes?

## Fixed corpus and arms
Use **the same six V13 SQL-sandbox cases**, only T0/T1 events. No public T2 repair events or oracle labels in model prompts. Use official pinned Qwen2.5-7B-Instruct-GGUF Q3_K_M model revision `293ca9a10157b0e5fc5cb32af8b636a88bede891`, pinned llama.cpp `ff5a23376725e5a889fa0bfb987b7541dc304da0`, temperature=0, seed=20261019, max_output_tokens=160, context=4096, CPU-only.

Exactly **24 real model completions**: six cases × two cutoffs × two conditions. Both use the SAME model, instruction, allowlisted causes and identical selected underlying raw event payloads. Conditions differ ONLY in event formatting:
- **L0 direct:** one compact chronological raw event JSON list (case and cutoff filtered).
- **L1 retrieval:** same exact event records selected by deterministic `case_id` + time cutoff from the broader event store, formatted as citation-labelled and timestamped retrieval snippets. This is a normal small exact-index retrieval, not semantic RAG. Both expose source IDs and timestamps in source JSON; token budgets and formatting lengths are not guaranteed equal.

Prompt chooses a single `H-A`, `H-B`, `H-C` or `none` with an explanation; do not calculate a numerical answer. One completion per question, fixed interleaved order; no gold answer, no fault injection target or repair data accessible by the inference runner.

All raw prompts and completions, input event IDs, response parse failures, usage, latency, model hash and sources logged. No repair of malformed model JSON. T0 must not see T1 or T2 data; T1 must not see T2.

## Evaluation
After all 24 model completions are frozen, read the separate internal intervention controller oracle to score per-arm correct cause vs author-assigned injected fault, useful coverage, incorrect definite proposals and T0→T1 selection changes. No automatic credit for abstention on valid but ambiguous T0; report safe withholding separately. Compare L0/L1 to pre-existing v13 R0/G1/G2 without mutating previous results. There is no expert-independent ground truth, production source authentication, or meaningful statistical superiority claim from n=6.

**Failure:** partial outputs, oracle exposed before completion, future event leakage, invented time labels, wrong model revision, non-executed language model, fabricated outcomes, or post-result prompt/threshold tuning. This is a **model-comparison engineering pilot**; external transfer and learning on unseen cases remain NOT_RUN.
