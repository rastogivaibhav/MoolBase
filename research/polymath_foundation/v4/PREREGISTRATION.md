# MOOL-POLYMATH-F4-001 — Frozen Actual-Model Pilot
Preregistered on 2026-10-18 **before downloading weights, invoking any model, or examining model results**.

## Purpose and interpretation
This study tests whether a *real pre-trained instruction model* can answer open, procedurally generated applied-domain tasks. It does **not** demonstrate MoolBase-specific reasoning, acquired competence from MoolBase, blind generalization, autonomous scientific discovery, or a polymath. Foundation v3 scoring checks remain the authority; no edits to its gold, cards, preregistration or scorer.

## Frozen sample and arms
Use fixed Foundation v3 generator seed **20261018** and first **8 ordered tasks per domain** (networks, biology, supply_chain) for **24 fixed tasks**. Preserve 72-task master SHA and 24-task subset SHA. No question selection by difficulty or outcomes. Report the actual number of valid/boundary problems after packet generation. Pilot is developmental and small; full v3 72 and independent hidden set remain untouched scientifically.

Use exactly the **same inference model** in both arms: `Qwen/Qwen2.5-0.5B-Instruct-GGUF`, `qwen2.5-0.5b-instruct-q4_k_m.gguf`, Apache-2.0, model revision `9217f5d` (record downloaded file's actual SHA-256); CPU inference through `llama.cpp` at commit `ff5a23376725e5a889fa0bfb987b7541dc304da0`.

- **A0** — Open-domain model: only task domain, requested target, input variables, card ID and fixed response format. No instructional equations.
- **A1** — Card-informed model: same task and response format, plus the corresponding original v3 project-authored card description, assumptions and boundary conditions. No oracle, answer or test labels.
- **A2/A3** — MoolBase native ontology/retrieval and HypoKosh/DWM: **NOT_RUN**, no substitutes may be reported as these systems without actual runtime integration.
- Previous `reference_oracle_not_learned` and `abstain_only_not_learned` results are instrument sanity bounds, not model arms.

## Fixed inference protocol
Use CPU-only local model, temperature=0, seed=20261018, context window 4096, max generated tokens **120**, same deterministic system instruction and output JSON schema for A0/A1; each task is a separate stateless request, no historical answers visible. Fixed trial order A0 for all 24 then A1 for all 24 (order effects are possible but no online training). Same model and output budget; **input tokens differ** because A1 reads cards, so inference cost is NOT compute matched. Record prompt, exact raw response, model revision/hash, serving revision, latency and token usage whenever available. No chain-of-thought capture requested.

Model JSON required: `answer_type=numeric|abstain`, `value` (if numeric) or meaningful `reason` (if abstain), `evidence_card_id`, `assumptions_acknowledged`, optional `confidence`. The task ID may be wrapped by the harness, but the harness may not repair a model's numeric answer, citation, answer type, or boundary choice. Malformed/nonparseable output counts as **incorrect**, not justified abstention: emit a synthetic rejected response with invalid citation and an error receipt, never conceal the failure. The scorer is unchanged and must receive 24 complete response records for the subset only. Do not relabel an error as successful abstention.

## Evaluation
Use the original v3 exact-answer oracle and exact card citation. Domain metrics: valid application accuracy, boundary abstention, numeric coverage, unsupported numeric assertions and citation validity. Report raw correct/total, per-domain and pooled; compute paired delta A1-A0 with 24 paired binary correct vectors. **No significance or superiority threshold** on this pilot. A0 and A1 correctness may both be poor due model size and strict formatting; record all null/negative results. External candidate model `model_executed=true` only after a real server response was recorded and bound to model file/hash. No fabricated results.

## Integrity and licensing
Open procedural questions and public scoring oracle make this a contaminated/easy-to-memorize benchmark, not independent blind competence. v3 cards are project-authored instructional examples, not original RFC/vendor/physiology text; no external source full-text training occurs. The official Qwen GGUF model card lists Apache-2.0; retain attribution, revision, SHA and artifact receipts. No paid GPU/hosted API, no secrets and no cloud deployment outside GitHub Actions.

## Acceptance and reporting
P0: preregistration precedes code/score.
P1: fixed 24-task subset, no gold in model prompts, no scoreboard before final scoring.
P2: model weights actually downloaded and inference server executed; both arms completed or report INCOMPLETE (never fill fabricated outputs).
P3: include every task's exact raw model response, structured response, parser errors, prompt, timing/usage, source and model SHA, exact GitHub commit, and grader receipts in a downloadable artifact.
P4: explicitly mark MoolBase+ontology and HypoKosh/DWM NOT_RUN; no claim of improved intelligence from two model prompting arms.
After this developmental pilot, commission separately hidden, rights-cleared expert questions and actual runtime integrations before claiming domain comprehension or cross-domain transfer.
