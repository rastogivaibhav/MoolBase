# EP-PROCESS-V2 — Cycle 1/2 Diagnostic Execution Prompt

## Role

Act as the senior maintainer, independent systems researcher, benchmark engineer, statistician, and adversarial verification lead for MoolBase.

Repository:
https://github.com/rastogivaibhav/graphenedb_v1

Research branch:
`lab/epistemic-process-v2-ten-cycle`

Frozen V1 score commit:
`3c008577739b83a94defc47c73275bf8ba76e76c`

Frozen experiment:
`EP-PROCESS-V1-SCORE-001`

## Mission

Complete the diagnostic phase that must precede any V2 production fix.

The work has two linked objectives:

1. Finish Cycle 1 by tracing every G2 challenge/reopen event from the frozen V1 score artifact and explain, using code and evidence, why zero revision transitions occurred.
2. Complete Cycle 2 by separating genuine system limitations from benchmark/measurement-contract effects, especially the G0 0% terminal-accuracy result.

Do not optimize MoolBase to improve a score. Do not modify V1. Do not alter production behavior during this activity. Do not reinterpret an inconvenient result as a bug unless evidence supports that classification.

## Non-negotiable scientific constraints

- Treat V1 as immutable evidence.
- Do not rerun V1 as a substitute for examining its preserved artifacts.
- Do not edit any file under `benchmarks/epistemic_process_v1/`.
- Do not change production C++ behavior during Cycle 1 or Cycle 2.
- Do not change evaluator semantics during this activity.
- Preserve negative findings.
- Distinguish:
  - observed fact,
  - code-path explanation,
  - measurement-contract consequence,
  - hypothesis requiring a new experiment.
- Do not call G0 a production regression merely because terminal accuracy is 0% if the configuration contract intentionally has no hypothesis-selection capability.
- Do not call G2 revision logic broken merely because no revisions occurred; determine whether the post-reopen evidence/ranking actually changed enough to satisfy the frozen revision condition.
- Any proposed fix must be deferred to a later V2 implementation cycle and recorded as a falsifiable hypothesis.

## Inputs

Use all of the following:

- immutable workflow artifact from EP-PROCESS-V1-SCORE-001
- `scored/raw-G2.json`
- `scored/receipts-G2.json`
- `scored/scores-G2.json`
- `scored/raw-G0.json`
- `scored/receipts-G0.json`
- `scored/scores-G0.json`
- `scored/aggregate-report.json`
- `benchmarks/epistemic_process_v1/tasks_v1.json`
- `benchmarks/epistemic_process_v1/run_production_v1.py`
- `benchmarks/epistemic_process_v1/adapt_v1.py`
- `benchmarks/epistemic_process_v1/evaluate_v1.py`
- `tools/epistemic_process_runtime_runner.cpp`
- `src/hypokosh_runtime_frontier_part_2.inc`
- `src/hypokosh_runtime_frontier_part_3.inc`
- `src/hypokosh_runtime_frontier_part_4.inc`
- `src/epistemic_control.cpp`
- relevant FiberBundle / dialectic-expansion code where required

## Cycle 1 — G2 causal diagnosis

For every G2 episode and every step:

1. Reconstruct the operative hypothesis before the decisive refutation/revocation.
2. Identify every emitted:
   - challenge,
   - reopen,
   - revision,
   - terminal event.
3. Record the challenged/reopened hypothesis nodes.
4. Record evidence refs and evidence-family identities attached to the events.
5. For each re-expansion, determine whether:
   - the bundle hash changed,
   - visited states increased,
   - max-hop/search options changed,
   - the set of candidate hypotheses changed,
   - support strength changed,
   - opposition strength changed,
   - target ranking changed,
   - `has_answer` changed,
   - `primary_node` changed.
6. Map each event to the production revision condition:

   `revised_answer = previous_has_answer != reopened_convergence.has_answer || (previous_has_answer && reopened_convergence.has_answer && previous_primary_node != reopened_convergence.primary_node)`

7. Explain why that predicate was false for each reopen that did not become a revision.
8. Classify each no-revision case as one of:
   - NO_NEW_FRONTIER
   - FRONTIER_CHANGED_NO_RANK_CHANGE
   - RANK_CHANGED_SAME_PRIMARY
   - ANSWER_STATE_UNCHANGED
   - EVENT_OR_RECEIPT_VISIBILITY_GAP
   - OTHER_WITH_EVIDENCE
9. Determine whether the lack of revisions is:
   - expected behavior under the current mechanism,
   - a production defect,
   - a task-design limitation,
   - insufficient evidence to decide.

Do not infer missing internal values. If the frozen receipt does not expose a value needed to prove causality, mark it as an observability gap and identify the minimal V2 diagnostic telemetry required.

## Cycle 2 — Measurement validity audit

Audit every primary V1 comparison and classify it.

At minimum evaluate:

### B0 vs G0

Determine whether terminal accuracy is a valid causal comparison given that:

- B0 directly emits a stateless hypothesis from the current observation.
- G0 intentionally executes Graphene evidence/provenance construction without HypoKosh hypothesis selection and the production runner explicitly projects it as open/abstain.

Answer:

- What capability was G0 actually designed to expose?
- Which V1 metrics legitimately measure that capability?
- Which metrics are structurally non-applicable or misleading for B0->G0?
- Is 0% coverage a genuine implementation failure, an expected contract consequence, or both in different senses?

### G0 vs G1

Determine what can actually be attributed to HypoKosh.

Separate:
- newly introduced hypothesis-selection capability,
- evidence/provenance behavior inherited from Graphene,
- measurement artifacts caused by comparing configurations with unequal output contracts.

### G1 vs G2

Determine what can actually be attributed to DWM.

Separate:
- challenge generation,
- reopen generation,
- evidence-frontier effects,
- terminal-answer effects,
- actual revision effects.

### Metric-by-metric applicability

For:
- evidence use
- refutation response
- revision inertia
- false convergence
- independent evidence convergence
- selective coverage
- receipt completeness
- execution cost

classify each layer comparison as:

- VALID_CAUSAL_COMPARISON
- VALID_DESCRIPTIVE_ONLY
- NON_APPLICABLE_BY_CAPABILITY_CONTRACT
- CONFOUNDED_BY_INTERFACE_DIFFERENCE
- INSUFFICIENT_EVIDENCE

Give a reason for every classification.

## Required deliverables

Create these files on the V2 branch:

1. `benchmarks/epistemic_process_v2/CYCLE_01_G2_REVISION_DIAGNOSIS.md`
   - immutable artifact provenance
   - per-episode trace
   - event counts
   - revision-predicate analysis
   - proven causes vs unresolved observability gaps
   - no production fixes

2. `benchmarks/epistemic_process_v2/CYCLE_02_MEASUREMENT_VALIDITY_AUDIT.md`
   - B0/G0/G1/G2 capability contracts
   - metric applicability matrix
   - valid vs invalid causal comparisons
   - corrected interpretation of V1 without changing V1
   - implications for V2 design

3. `benchmarks/epistemic_process_v2/V2_DIAGNOSTIC_FINDINGS.json`
   Machine-readable findings with:
   - source V1 commit
   - artifact digest/id if available
   - episode findings
   - G0 classification
   - G2 no-revision classifications
   - observability gaps
   - hypotheses proposed for later cycles
   - explicit `production_code_changed: false`
   - explicit `v1_modified: false`

4. `benchmarks/epistemic_process_v2/V2_PREREGISTRATION_INPUTS.md`
   Only list evidence-backed questions/hypotheses that the later V2 preregistration should test. Do not yet choose success thresholds from observed V2 data.

## Acceptance gates

Cycle 1 is complete only if:
- every G2 challenge/reopen in the frozen artifact is accounted for;
- zero revisions are explained as far as preserved telemetry permits;
- unknowns are labelled unknown rather than guessed;
- no production code changed.

Cycle 2 is complete only if:
- G0's 0% score is correctly separated from its actual capability contract;
- every main V1 metric has a layer-comparison applicability classification;
- the report does not erase or rewrite V1 results;
- concrete V2 design implications are stated without tuning against V1 answers.

## Final action

Commit only the diagnostic artifacts to `lab/epistemic-process-v2-ten-cycle`.

Then compare the branch against frozen master and verify that:
- all changes are under `benchmarks/epistemic_process_v2/`;
- no V1 file changed;
- no production source/header changed.

If that boundary fails, stop and repair the branch before declaring Cycle 1/2 complete.
