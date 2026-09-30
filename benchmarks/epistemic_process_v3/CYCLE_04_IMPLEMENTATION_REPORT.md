# EP-PROCESS-V3 — Cycle 4 Implementation Report

Status: **IN PROGRESS — AWAITING MECHANICAL CI**

## Objective

Implement only the minimum production changes justified by the V2/V3 Cycle-1 root causes and frozen Cycle-2/Cycle-3 scientific contract.

No V3 score-bearing outcomes may be computed in this cycle.

## Frozen Experiment Boundary

The following remain unchanged and hash-protected:

- Cycle-2 semantic contract
- measurement model
- metrics
- telemetry contract
- claim gates
- failure policy
- statistics plan
- task-family requirements
- controls
- V3 generator
- 384-episode task candidate

Task candidate SHA-256:

`575c89930859bc6f23ecd218c7ed4c27032ec3a9f5a8c12bea1bbe84ab356829`

## Implemented Mechanisms

### 1. Identity-invariant ties

Exact semantic equality now returns no operative target rather than falling through to numeric target ID.

Frozen combined tolerance:

- absolute: `1e-9`
- relative: `1e-6`

The runtime now has an explicit `Open` governed state for unresolved semantic ties.

### 2. Refuted incumbent / under-corroborated replacement

When a historical support leader is materially opposed and the semantic replacement has not yet earned two independent support families, the old incumbent is no longer restored by the corroboration safeguard.

The result remains without an operative target and projects to `Contested`.

### 3. DWM challenge vs corroboration

A separately supported alternative is no longer automatically a dialectical challenge.

- material opposition → DWM `Challenge`
- insufficient independent support → HypoKosh `CorroborationSearch`

### 4. Expansion-opportunity gating

A DWM reopen now requires production-observable search opportunity.

Currently implemented opportunity classes:

- `NEW_ELIGIBLE_NODE_AVAILABLE`
- `BUDGET_BOUND_AND_EXPANDABLE`
- `NO_EXPANSION_OPPORTUNITY`

Challenge may still be recorded when the frontier is exhausted, but no `Reopen` is emitted.

### 5. Native transition telemetry

The runtime event surface now includes:

- `CorroborationSearch`
- `Decommitment`
- `Recommitment`
- `Resolution`

Existing native `Revision`, `Challenge` and `Reopen` events remain.

Commitment-layer transitions are emitted only after governed/Lyapunov status is finalized for the recovery round.

### 6. Evidence lifecycle

Evidence provenance now retains an explicit lifecycle:

- `active`
- `refuted`
- `revoked`
- `superseded`
- `invalidated`
- `audit_only`

Unknown lifecycle values fail closed as `invalidated`.

Non-operative evidence stays in FiberBundle audit evidence and participates in the immutable state hash, but paths depending on it cannot contribute active support or opposition.

## Focused Mechanical Contracts

A new real-GrapheneDB Cycle-4 contract test covers:

1. exact tie → `Open`, no target-ID winner;
2. insufficiency → `CorroborationSearch`, no DWM challenge/reopen;
3. material opposition + exhausted frontier → Challenge, no Reopen;
4. material opposition + unseen admissible downstream node → Challenge + Reopen;
5. superseded evidence remains auditable but does not count as active support.

## Legacy Test Corrections

Old tests that encoded V2 behavior were updated only where the frozen V3 semantics explicitly changed the contract:

- exact tie no longer expects lower target ID;
- semantic verification now breaks a previously unresolved tie;
- supported alternatives alone no longer count as opposition;
- monkey dialectic family now injects actual contradiction.

## Negative Findings

An earlier GA-readiness run exposed old M10 semantics and old DWM competition assumptions. These were preserved as evidence and corrected at the test-contract level.

## Repository State

To be sealed after successful dedicated Cycle-4 CI.

## Gate

**INCOMPLETE — mechanical validation pending**

## Next Authorized Activity

Only after Cycle 4 passes: run a blind **unscored mechanical campaign** against the frozen V3 runtime view to test schema/telemetry compatibility and mechanism execution without joining outputs to the oracle.
