# MOOL-POLYMATH-F3-001 — Frozen Applied-Comprehension Evaluation Protocol

**Preregistered 2026-10-18 before implementing the v3 generator, grader or evaluation.**

## Research question

Can a future MoolBase-backed learner **apply** separately acquired explanatory concepts to unfamiliar numerical situations, identify boundaries where the learned model is inapplicable, and cite an appropriate explanatory source? The present v3 work builds the *measurement instrument* and tests the instrument itself. It does NOT claim that a model has acquired this competence.

## Source and rights scope

Use only original, newly authored pedagogical mechanism cards (simple domain mathematical models) written for this research branch; label these `project_authored_instructional` and `license_review_required_before_external_republication` in addition to the existing repository Apache-2.0 code licence. These cards are *not* transcripts or official statements of the RFC, physiology or vendor sources. Historical v2 reference notes remain citations for human inspection only; they carry `allow_model_training=false` and are NOT upgraded to publisher-approved corpus material.

The optional public SciQ evaluation dataset is explicitly **not included**, because its published CC-BY-NC-3.0 rights may not fit all deployment contexts. Existing public test questions, including SciQ, can also be contaminated by pretraining, so they would not establish clean independent proficiency.

## Fixed task families and boundaries

Three separate reading cards (one per domain):

1. **Network queue (simplified discrete queue):** input `queue`, `arrivals`, `service`, `buffer` (nonnegative integers, `queue<=buffer`). Compute `unserved=max(0, queue+arrivals-service)`; `next_queue=min(buffer,unserved)`; `drops=max(0,unserved-buffer)`. This is NOT real TCP congestion-control implementation.
2. **Biological regulation (toy proportional homeostat):** finite scalars `observed`, `setpoint`, `gain>=0`, `limit>0`, `disturbance`; `control=clip(gain*(setpoint-observed),-limit,+limit)`; `next_state=observed + disturbance + control`. It does NOT model a real organism or claim all biological regulation follows a linear law.
3. **Supply replenishment (toy reorder policy):** nonnegative integer `daily_demand`, `lead_days`, `safety`, `on_hand`, `on_order`, `backorders`, `target`; compute `position=on_hand+on_order-backorders`; `reorder_point=daily_demand*lead_days+safety`; order `max(0,target-position)` when `position<=reorder_point`, else 0. Not an ERP implementation guarantee.

Two numeric response types per domain:
- networks: `next_queue`, `drops`;
- biology: `control`, `next_state`;
- supply: `reorder_point`, `order_qty`.

Each domain also has deliberately **out-of-contract** cases: negative arrivals or invalid buffer; negative controller gain or nonpositive control limit; negative daily demand or unknown lead_days. These require `abstain` and explanation of model boundary, not a fabricated numeric answer.

## Reproducible task design

The challenge generator shall create **20 valid cases + 4 boundary cases per domain**, or **72 tasks**: 60 valid numerical tasks and 12 boundary cases. Use Python `random.Random(20261018)` with an independent deterministic per-domain random stream to generate scenarios from prespecified integer/half-step ranges. Target family and domain are visible; each task must include only input variables, `card_id`, problem text and opaque task ID. **No gold answers, executable solver logic, or rubric-derived result may appear in exported challenge packets.**

The generator and scoring oracle are in the same public repo, so the exercise is **not independently blind** and cannot establish novelty or resistance to memorisation. New parameters differ from the reading examples, which tests arithmetic application rather than exact text recall. Passing a reference oracle is a self-test of scoring instrumentation, not evidence a system understands concepts.

## Prediction contract and baselines

Student submissions are keyed by task ID, include:
- `answer_type = numeric | abstain`;
- `value` for numeric answers (finite number);
- `evidence_card_id` matching the task's allowed reading card;
- `assumptions_acknowledged` boolean;
- `reason` for abstention;
- `confidence` in [0,1] (optional, descriptive only).

Scoring compares submitted response to a private-in-process deterministic numerical oracle, and requires valid citation metadata. Numeric tolerance `1e-9` absolute; integer tasks must retain exact integer values. Boundary cases earn credit only for an explicit abstention with a meaningful reason. Missing submissions, extra IDs, invalid evidence, nonfinite values and type mismatches fail submission validation. Empty submissions are allowed only in a special `not_evaluated` instrument smoke mode, never scored as model attempts.

Report per-domain and pooled exact correctness, boundary abstention, unsupported assertion rate and coverage. No accuracy claim is earned unless an independently executed candidate is actually submitted. Benchmark arms:
- **B0 abstain-only**, checks that avoiding assertions cannot earn numerically scored comprehension.
- **B1 scripted reference solver**, checks that generated challenges have precise, solvable answers and that the grader catches errors. It has access to the oracle algorithm and is **not a learned model**.
- **B2 external learned candidate**, **NOT_RUN until a model/harness is connected** and the tasks and source usage permissions are reviewed.

All methods share fixed challenge packets and one answer key under the same seed; no result-based tuning.

## Technical acceptance

G0: card and task source descriptions carry licenses and provenance boundaries; no external full-text ingestion.
G1: task generator produces exactly 72 tasks, no answers leak into packets, stable hashes on repeats, deterministic seed and 3 distinct domains.
G2: reference scorer correct on all cases; deliberate tampering with answer, citation, boundary response or task ID is detected and rejected/penalised.
G3: no `Verified` facts are inserted in MoolBase; all generated tasks remain pedagogical and any agent outcomes remain unverified until independent assessment.
G4: optional candidate scores only if actual submitted predictions are auditable; no fake model results.

## Next independent learning gate

Commission a separate, independently authored hidden application and counterexample set, with real rights-cleared source excerpts and human/independent evaluation. The hidden set must not be accessible to the learner or to implementers tuning prompts/models. Compare fixed LLM, retrieval-only, ontology-informed, and full HypoKosh–DWM learning on equal compute. Hold out one domain entirely to assess transfer, then test candidate discovery and question selection under a separately preregistered protocol.

This v3 milestone is an **assessment infrastructure gate**, not a validated polymath intelligence result. Preserve negative outcomes and do not revise this protocol after implementation.
