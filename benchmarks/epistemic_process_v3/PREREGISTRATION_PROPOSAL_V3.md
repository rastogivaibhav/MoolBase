# EP-PROCESS-V3 — Proposed Preregistration Inputs from Cycle 1

Status: **DRAFT INPUT TO CYCLE 2 — NOT YET FROZEN**

This document converts Cycle-1 root causes into falsifiable V3 hypotheses.
It is not the final V3 preregistration and contains no score authorization.

## Proposed hypotheses

### H-TIE-INVARIANCE

For genuinely symmetric evidence, the semantic result is invariant to:

- H1/H2 naming;
- target insertion order;
- numeric node IDs;
- evidence insertion order;
- evidence-family names.

A symmetric unresolved tie must not select a hypothesis solely because it owns the lower node ID.

### H-PERSIST-VALID

Persistent Graphene state preserves currently valid evidence and provenance across observations.

History-dependent performance should improve or remain non-inferior relative to the same decision head without persistent state.

### H-INVALIDATION

Evidence explicitly declared revoked/superseded/invalid for operative reasoning no longer contributes positive active target strength, while remaining auditable if the storage model requires historical retention.

### H-REFUTATION-SEMANTICS

Material refutation is represented distinctly from deletion.

The frozen V3 semantic contract must specify whether refuted support remains operative, becomes audit-only, or is superseded.

### H-INCUMBENT-REPLACEMENT

When an incumbent is materially refuted and a replacement lacks sufficient independent corroboration, behavior must follow one preregistered rule rather than falling back to numeric/order artifacts.

Candidate rule to evaluate in Cycle 2:

**contested + no committed answer + no stale incumbent promotion until replacement earns its required support.**

Whether an operative provisional replacement is allowed must be frozen before implementation.

### H-DWM-OPPORTUNITY

DWM reopens only when a frozen opportunity predicate indicates that the search frontier can plausibly change.

Negative controls with exhausted frontiers should not generate reopen activity.

### H-DWM-PRECISION

DWM challenge events correspond to actual opposition/contradiction semantics, not merely lack of corroboration.

Evidence-insufficiency recovery is reported separately.

### H-DWM-USEFULNESS

On preregistered expansion-available cases, DWM reopens produce measurable downstream change in at least one of:

- frontier;
- discovered evidence;
- target ranking;
- operative hypothesis;
- governed status;
- commitment.

### H-REVISION

When:

1. an initial belief is rationally supported;
2. later decisive evidence invalidates it;
3. adequate independent replacement evidence becomes available;

the runtime emits a native revision transition within the preregistered latency bound.

### H-COVERAGE-SAFETY

A DWM safety gain is not sufficient for a broad outcome claim if commitment coverage/correct-commitment yield collapses beyond the frozen tolerance.

## Proposed controls

Retain:

- C0 — stateless common-head control;
- G0E — Graphene evidence/provenance only;
- C1 — Graphene persistent state + common head;
- G1 — Graphene + HypoKosh;
- G2 — Graphene + HypoKosh + DWM.

Add diagnostic ablations only if Cycle 2 can justify them causally, for example:

- a governed-selection control that removes only the incumbent corroboration safeguard;
- a DWM opportunity-gate control;
- challenge-without-reopen diagnostic.

Do not add a control merely to improve apparent V3 performance.

## Proposed task families

The V3 universe should contain separate populations for:

1. exact symmetric tie;
2. near tie;
3. H1/H2 mirror;
4. target insertion-order permutation;
5. node-ID permutation;
6. evidence-order permutation;
7. family-name permutation;
8. duplicate/correlated support;
9. valid persistent history;
10. explicit revocation;
11. material refutation without replacement;
12. material refutation with insufficient replacement;
13. material refutation with sufficient replacement;
14. delayed sufficient replacement;
15. explicit supersession;
16. correlated majority vs independent minority;
17. DWM latent-evidence / useful-expansion positive control;
18. DWM exhausted-frontier negative control;
19. DWM evidence-insufficiency-but-no-opposition control;
20. genuine earned-resolution sequence;
21. single-step control;
22. no-change control.

Mirror every hypothesis-sensitive family H1/H2.

## Proposed DWM opportunity telemetry

For every contemplated reopen record:

- trigger class;
- challenge class;
- reopen nodes;
- frontier exhausted before reopen?;
- unseen eligible node count if knowable;
- semantic candidate budget before/after;
- path budget before/after;
- visited-state budget before/after;
- candidate-set delta;
- path-set delta;
- evidence-ref delta;
- bundle-hash delta;
- rank delta;
- operative delta;
- status delta;
- commitment delta;
- stop reason.

Suggested opportunity classes:

- `NEW_ELIGIBLE_NODE_AVAILABLE`
- `NEW_ADMISSIBLE_PATH_AVAILABLE`
- `BUDGET_BOUND_AND_EXPANDABLE`
- `DEPENDENCY_STATE_CHANGED`
- `EXTERNAL_EVIDENCE_ARRIVED`
- `FRONTIER_EXHAUSTED`
- `NO_EXPANSION_OPPORTUNITY`

## Proposed measurement correction

Production FiberBundle family lineage is namespace-qualified.

V3 evaluator expected family IDs should use the same canonical representation, e.g.:

`family:<raw-task-family-id>`

Do not change the frozen V2 evaluator or V2 evidence.

## Proposed safety/utility reporting

Avoid relying on one weighted score as the primary claim gate.

Report at least:

- operative accuracy;
- commitment coverage;
- correct-commitment yield;
- committed accuracy;
- false-commitment incidence;
- appropriate abstention;
- inappropriate abstention;
- earned resolution;
- challenge precision;
- challenge rate;
- unnecessary reopen rate;
- reopen usefulness;
- actual revision rate;
- execution cost.

Preferred broad-outcome gate:

a preregistered multi-dimensional non-inferiority/Pareto rule in which lower false commitment cannot compensate for an excessive collapse in correct-commitment yield or coverage.

## Thresholds still to freeze in Cycle 2

Cycle 1 intentionally does not freeze numerical thresholds.

Cycle 2 must decide before implementation/scoring:

- tie-symmetry tolerance;
- persistence non-inferiority margin;
- single-step-control regression margin;
- challenge-precision threshold;
- unnecessary-reopen maximum;
- reopen-usefulness minimum;
- actual-revision minimum;
- commitment-coverage / correct-yield non-inferiority margin;
- earned-resolution threshold;
- statistical alpha;
- multiple-testing family;
- bootstrap seed/count;
- failure policy.

## Proposed implementation rule

No production fix may enter V3 until its corresponding:

1. V2 failure;
2. Cycle-1 root cause;
3. V3 hypothesis;
4. negative control;
5. regression detector

are all explicitly linked in the frozen preregistration.
