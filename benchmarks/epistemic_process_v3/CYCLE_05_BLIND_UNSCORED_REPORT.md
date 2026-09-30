# EP-PROCESS-V3 — Cycle 5 Blind Unscored Mechanical Campaign

Status: **IN PROGRESS — AWAITING BLIND CAMPAIGN**

## Objective

Execute the frozen 384-episode V3 universe through the revised production variants without evaluator truth, correctness joins, claim scoring, or task-family labels.

## Execution Matrix

Planned per pass:

- G0E: 384 episodes
- G1: 384 episodes
- G2: 384 episodes
- total: **1,152 production executions**

The exact same campaign is run twice with seed `20261004`.

## Blind Boundary

The campaign input retains only:

- episode ID / variant number;
- target insertion order and padding;
- runtime-visible evidence events;
- initial search limits;
- harness-only latent graph topology required to stage the graph.

It removes:

- task family;
- mirror labels;
- scientific-purpose text;
- claim IDs;
- evaluator oracle;
- expected answers/statuses;
- revision/resolution eligibility;
- challenge/usefulness labels.

## Mechanical Pass Conditions

Cycle 5 passes only if:

- all 1,152 executions complete per pass;
- G0E/G1/G2 each contain exactly 384 unique episodes;
- no runtime failure occurs;
- no telemetry gap occurs;
- recovery-round telemetry satisfies the V3 fields;
- G2 emits real Challenge events;
- G2 emits CorroborationSearch events;
- G2 emits Reopen events;
- G1 emits no DWM Challenge/Reopen events;
- G2 observes both expandable and exhausted-frontier opportunities;
- at least one latent evidence item is actually discovered by G2;
- pass 1 and pass 2 are deterministic after excluding timing;
- no oracle/outcome field appears in campaign input/output;
- no correctness, accuracy, claim status or score is calculated.

## Frozen Boundary

Cycle 5 must not change:

- `src/`
- `include/`
- `tests/`
- the 384-task candidate;
- V3 generator;
- Cycle-2 semantic/metric/statistical/claim contracts.

## Gate

**INCOMPLETE — blind campaign pending**

## Next Authorized Activity

If Cycle 5 passes: inspect only the mechanical report for missing mechanism coverage. If mechanically complete, proceed to V3 evaluator/score-boundary freeze without reading score outcomes.
