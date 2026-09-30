# EP-PROCESS-V2 — Cycle 3 Acceptance Checklist

Cycle 3 is complete only if every item below is true.

## Scientific design

- [x] Operative hypothesis and committed answer are separate concepts.
- [x] Cross-step belief change and within-call revision are separate events.
- [x] Historical G0 is not scored as though it had a hypothesis selector.
- [x] A same-common-head C0→C1 pair exists to isolate persistence.
- [x] G1→G2 remains the clean DWM comparison.
- [x] Primary metrics are preregistered.
- [x] Claim thresholds are frozen before V2 score-bearing output exists.
- [x] Statistical treatment is preregistered.
- [x] Failure handling is preregistered.
- [x] Oracle-visible fields are explicitly forbidden from runtime input.

## Telemetry

- [x] Previous-step state is required.
- [x] Initial and final state snapshots are required.
- [x] Recovery-round before/after bundle hashes are required.
- [x] Per-target support/opposition/belief/rank telemetry is required.
- [x] Search-option deltas are required.
- [x] Challenge/reopen/revision/commitment transitions are distinct.
- [x] Deterministic canonical telemetry is required.

## Experiment boundary

- [x] V1 is not modified.
- [x] Production behavior is not modified in Cycle 3.
- [x] No score-bearing V2 run is authorized.
- [x] Any later alteration after V2 scoring requires a new experiment id.

## Next authorized activity

Cycle 4 may implement the preregistered telemetry and C0/C1 common-head test harness.

Cycle 4 must **not** tune G1/G2 behavior for score improvement.

Implementation is acceptable only if it realizes this frozen measurement contract without changing the scientific questions.
