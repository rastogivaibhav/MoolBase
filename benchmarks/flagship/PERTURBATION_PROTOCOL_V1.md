# Flagship Perturbation Protocol V1

Status: **pre-registered implementation contract**

Parent issue: #36  
Programme: #25  
Canonical merged flagship parent: `51b2908f869547617283ccd8c12ea0520026810d`

## Why this exists

The canonical flagship proof is now frozen as a mechanism demonstration. This protocol does not replace it and does not widen its claims.

The goal is to attack the same epistemic invariants with controlled perturbations and preserve every outcome, including failures, before any repair is attempted.

The canonical flagship mechanism receipt remains:

```text
36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef
```

Neither that hash nor `benchmarks/flagship/scenario.json` may be changed by this work.

## Perturbations

### P1 — duplicate-family injection

Add another graph-distinct support path whose evidence resolves to an already represented family.

Expected invariant:

```text
raw path count may rise
independent family count must not rise
duplication alone must not earn corroboration
duplication alone must not unlock final resolution
```

Any violation is a P0 false-convergence defect.

### P2 — decisive independent-family removal

Remove one genuinely independent support family from a previously corroborated state.

Expected invariant:

```text
independent family count falls
required corroboration falls with it
raw/path multiplicity cannot substitute for the removed family
```

Any silent persistence of corroboration is P0.

### P3 — material contradiction injection

Inject material opposition against the leading hypothesis.

Expected invariant:

```text
contradiction remains visible
final resolution is blocked under the current flagship contract
opposition/reopen state remains inspectable
```

A definitive result that survives without an explicit admissibility explanation is P0.

### P4 — deterministic ingestion-order permutation

Keep evidence identities and semantics fixed while permuting insertion/path order.

Expected invariant:

```text
epistemic outcome remains equivalent
order alone cannot change convergence
receipt differences must be attributable to declared provenance/order fields
```

Order-sensitive epistemic state is P1; order-only final-convergence change is P0.

### P5 — bounded search/depth alteration

Change search/depth budgets without changing evidence.

Expected invariant:

```text
the receipt exposes the altered budget/frontier decision
the runtime must not compensate by silently widening another dimension
insufficient-budget uncertainty/non-convergence is a valid result
```

Silent widening is P0. Unexplained state drift is P1.

## Evidence rules

Every perturbation must produce a deterministic receipt containing:

- canonical parent commit;
- perturbation ID and manifest hash;
- changed control/evidence inputs;
- observed epistemic state;
- expected-contract evaluation;
- PASS or FAIL;
- if FAIL, failure class and implementation priority.

The aggregate report must include **all five** perturbations. A failing perturbation must not be removed from the report after an implementation repair.

If a failure causes a code change:

1. preserve the failing receipt;
2. document root cause;
3. implement the narrow repair;
4. produce a new post-fix receipt;
5. prove the canonical flagship mechanism receipt remains unchanged.

## Non-claims

Passing this protocol does not establish semantic truth, automatic hidden-dependence discovery, autonomous scientific discovery, durable cross-run DWM belief promotion, general superiority over Jev or other systems, or enterprise readiness.

## Exit gate

V1 exits only when P1–P5 all execute from a clean checkout and the repository preserves every expected/observed result plus any failure-driven implementation priorities.
