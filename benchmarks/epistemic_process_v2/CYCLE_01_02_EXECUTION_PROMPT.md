# EP-PROCESS-V2 — Cycle 1/2 Diagnostic Execution Prompt

## Role

Act as the senior maintainer, independent systems researcher, benchmark engineer, statistician, and adversarial verification lead for MoolBase.

Repository:
https://github.com/rastogivaibhav/graphenedb_v1

Research branch:
`lab/epistemic-process-v2-cycle1-2-diagnostics`

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
- Distinguish observed fact, code-path explanation, measurement-contract consequence, and hypothesis requiring a new experiment.
- Do not call G0 a production regression merely because terminal accuracy is 0% if the configuration contract intentionally has no hypothesis-selection capability.
- Do not call G2 revision logic broken merely because no revisions occurred; determine whether the post-reopen evidence/ranking actually changed enough to satisfy the frozen revision condition.
- Any proposed fix must be deferred to a later V2 implementation cycle and recorded as a falsifiable hypothesis.

## Inputs

Use the immutable EP-PROCESS-V1-SCORE-001 artifact and the frozen V1 implementation, especially:

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

## Cycle 1 — G2 causal diagnosis

For every G2 episode and every step:

1. Reconstruct the operative hypothesis before the decisive refutation/revocation.
2. Identify every challenge, reopen, revision and terminal event.
3. Record challenged/reopened hypothesis nodes.
4. Record evidence refs and evidence-family identities.
5. Determine, only where preserved evidence permits, whether bundle, search frontier, ranking or answer changed.
6. Map every reopen to the frozen production revision predicate:

```cpp
revised_answer =
    previous_has_answer != reopened_convergence.has_answer ||
    (previous_has_answer && reopened_convergence.has_answer &&
     previous_primary_node != reopened_convergence.primary_node)
```

7. Explain why that predicate was false for every reopen without a revision.
8. Classify no-revision cases using:
   - `NO_NEW_FRONTIER`
   - `FRONTIER_CHANGED_NO_RANK_CHANGE`
   - `RANK_CHANGED_SAME_PRIMARY`
   - `ANSWER_STATE_UNCHANGED`
   - `EVENT_OR_RECEIPT_VISIBILITY_GAP`
   - `OTHER_WITH_EVIDENCE`
9. Mark any unobservable internal value as an observability gap. Do not infer it.

## Cycle 2 — Measurement validity audit

Audit B0→G0, G0→G1 and G1→G2 for:

- evidence use
- refutation response
- revision inertia
- false convergence
- independent evidence convergence
- selective coverage
- receipt completeness
- execution cost

Classify each comparison as one of:

- `VALID_CAUSAL_COMPARISON`
- `VALID_DESCRIPTIVE_ONLY`
- `NON_APPLICABLE_BY_CAPABILITY_CONTRACT`
- `CONFOUNDED_BY_INTERFACE_DIFFERENCE`
- `INSUFFICIENT_EVIDENCE`

Explicitly explain the G0 capability contract and whether its 0% terminal coverage is a real implementation failure, expected contract consequence, or different things under different interpretations.

For G1→G2, separate challenge generation, reopen generation, frontier effects, terminal-answer effects, status/calibration effects and actual revision effects.

## Required deliverables

Create only:

1. `benchmarks/epistemic_process_v2/CYCLE_01_G2_REVISION_DIAGNOSIS.md`
2. `benchmarks/epistemic_process_v2/CYCLE_02_MEASUREMENT_VALIDITY_AUDIT.md`
3. `benchmarks/epistemic_process_v2/V2_DIAGNOSTIC_FINDINGS.json`
4. `benchmarks/epistemic_process_v2/V2_PREREGISTRATION_INPUTS.md`

Also preserve this execution prompt as:
`benchmarks/epistemic_process_v2/CYCLE_01_02_EXECUTION_PROMPT.md`

## Acceptance gates

Cycle 1 is complete only if every frozen G2 challenge/reopen is accounted for, zero revisions are explained as far as preserved telemetry permits, unknowns remain unknown, and no production code changes.

Cycle 2 is complete only if G0's score is separated from its capability contract, every primary metric receives an applicability classification, V1 results remain unchanged, and V2 implications are phrased as hypotheses rather than tuned fixes.

## Final boundary check

Compare the branch against frozen V1 commit `3c008577739b83a94defc47c73275bf8ba76e76c`.

The only changed files must be under `benchmarks/epistemic_process_v2/`.

If any production source/header, CMake file, workflow, or V1 benchmark file changes, the diagnostic boundary fails and the work must not be declared complete.
