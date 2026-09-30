# EP-PROCESS-V2 — Preregistration Inputs from Cycles 1–2

Status: evidence-backed questions only. This is not yet the V2 preregistration.

## H1 — Graphene persistence/provenance
Does the Graphene-only layer retain and expose historical evidence/provenance more faithfully than a stateless baseline under duplicates, dependency, contradiction, revocation and reordering?

V1 basis: G0 accumulated all ingested evidence refs while B0 retained only the current observation.

## H2 — Common-decision-head ablation
When answer-producing configurations use an explicitly common decision interface, what causal contribution is attributable to persistent Graphene state vs HypoKosh selection?

Guardrail: do not redefine historical G0. Use a separately named V2 profile.

## H3 — Cross-step belief change
Does decisive new evidence cause an explicit H1→H2 or H1→ABSTAIN event at the observation boundary?

V1 basis: EP01, EP03 and EP04 changed operative hypothesis across calls without a revision event.

## H4 — DWM reopen usefulness
Does a DWM reopen alter frontier, ranking, status calibration, confidence or committed answer relative to otherwise identical G1 execution?

V1 basis: 19 challenge/reopen pairs, four additional expansion rounds, zero revisions, and matching final visited-state/selected-evidence counts.

## H5 — DWM challenge precision
Are challenges concentrated on genuinely ambiguous/contradicted cases rather than firing mechanically under incomplete support?

V1 basis: G2 challenged/reopened on 19 of 20 observation steps.

## H6 — Commitment calibration
Does G2 appropriately move to contested/evidence-required/abstain when replacement evidence is insufficient?

V1 basis: EP04 expected ABSTAIN but produced open/contested H2.

## H7 — Refutation response semantics
What constitutes a valid response: reopen, hypothesis change, commitment downgrade, abstention, or a preregistered combination?

V1 basis: EP02 got response credit for H1→H1 reopen, while EP01/EP03 prompt H1→H2 changes received no credit.

## H8 — Cost/value tradeoff
What runtime/search cost is attributable to HypoKosh and DWM, and what epistemic effect justifies it?

## Freeze rule

No V2 score-bearing run until:
- broader task families are frozen;
- capability contracts are explicit;
- common vs capability-specific metrics are declared;
- cross-step and within-call transitions are separated;
- diagnostic telemetry is frozen;
- evaluator and task hashes are recorded;
- success/effect criteria are preregistered before score-bearing outputs are inspected.
