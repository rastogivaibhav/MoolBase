# Cycle 01 — G0 Forensic Diagnosis

## Question

Why did V1 report 0% G0 coverage and 0% terminal accuracy?

## Source trace

The V1 score path sets `options.enable_hypokosh = configuration != "G0"` and `enable_dwm = configuration == "G2"` in `tools/epistemic_process_runtime_runner.cpp`.

When HypoKosh is disabled, `CompleteHypoKoshRuntime::reason` enters an explicit G0 branch in `src/hypokosh_runtime_frontier_part_2.inc`.

That branch:

- executes `GrapheneEvidenceExpander`;
- builds a `FiberBundle`;
- preserves evidence targets, evidence edges and evidence-family identity;
- marks Graphene execution and the bundle as authoritative;
- intentionally does **not** execute stability/admissibility, convergence, DWM opposition, recovery, or hypothesis selection;
- sets `status = Abstain`, `primary_node = 0`, and terminal cause `g0_evidence_projection_no_hypothesis_selection`;
- emits a GrapheneCore terminal event with epistemic state `abstain`.

This is an explicit capability boundary, not an accidental empty result.

## Measurement boundary discovered

The V1 runtime runner does not expose that native G0 terminal event to the V1 adapter. Instead, for every G0 step it writes a direct benchmark decision:

```text
status = open
hypothesis = null
```

The V1 adapter is explicitly written to use direct decisions for B0/G0 and native events only for G1/G2. Therefore the evaluator receives `open + null hypothesis`, not the runtime's native `abstain` state.

## Causal conclusion

V1's measured G0 terminal regression is real **under the V1 benchmark contract**, but it is not evidence that Graphene failed to persist or retrieve evidence.

The result combines two facts:

1. G0 is intentionally an evidence/provenance capability with no hypothesis-selection layer.
2. The V1 benchmark projected its native abstention as `open`, which made terminal-answer comparison against B0 especially uninformative.

## What V1 still established

- G0 executed Graphene and created auditable evidence state.
- G0 was not a decision-making replacement for B0.
- G0 terminal accuracy should not be used as the primary metric for the causal value of Graphene-only persistence.

## V2 requirement

G0 must be scored primarily on evidence/provenance properties such as evidence retention, evidence-family identity, correlation handling, retrieval integrity, deterministic replay, and audit completeness.

If terminal semantics are recorded, V2 must preserve G0's native `abstain` rather than silently converting it to `open`.

## Status

**Cycle 01 exit gate: PASS.** Root cause is identified without modifying V1.
