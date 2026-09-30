# EP-PROCESS-V2 — Preregistration Inputs from Cycles 1–2

Status: evidence-backed questions only. This document does not set V2 score thresholds and is not itself the V2 preregistration.

## H1 — Graphene persistence/provenance

**Question:** Does the Graphene-only layer retain and expose historical evidence/provenance more faithfully than a stateless baseline under duplicates, dependency, contradiction, revocation and reorderings?

**V1 basis:** G0 accumulated all ingested evidence refs while B0 retained only the current observation.

**Required V2 endpoint:** evidence-retention/provenance correctness, independent of terminal-answer accuracy.

## H2 — Common-decision-head ablation

**Question:** When B0/G0/G1/G2 are compared through an explicitly common answer interface, what causal contribution is attributable to persistent Graphene evidence state vs HypoKosh selection?

**V1 basis:** G0 had no hypothesis-selection capability, so B0→G0 and G0→G1 terminal accuracy were interface-confounded.

**Guardrail:** do not redefine historical G0; create a separately named V2 profile if a common decision head is introduced.

## H3 — Cross-step belief change

**Question:** Does newly arriving decisive evidence cause a recorded H1→H2 or H1→ABSTAIN change at the correct observation boundary?

**V1 basis:** EP01, EP03 and EP04 changed operative hypothesis across calls without emitting a revision event.

**Required telemetry:** previous-step operative state, current initial state, causal evidence ids and an explicit cross-step belief-change event.

## H4 — DWM reopen usefulness

**Question:** Does a DWM-triggered reopen change the evidence frontier, target ranking, confidence/status calibration or committed answer relative to an otherwise identical G1 call?

**V1 basis:** G2 emitted 19 challenge/reopen pairs and four additional expansions but no revisions; final visited-state and selected-evidence counts matched G1.

**Required telemetry:** initial/reopened bundle hashes, target scores/ranks, search-option deltas, visited states and selected evidence.

## H5 — DWM challenge precision

**Question:** Are DWM challenges concentrated on genuinely ambiguous/contradicted cases rather than mechanically firing whenever support is incomplete?

**V1 basis:** G2 challenged/reopened on 19 of 20 observation steps, including early single-support states.

**Required endpoint:** challenge precision/necessity against preregistered challenge-worthy structural conditions.

## H6 — Commitment calibration

**Question:** Does G2 appropriately move from committed/provisional answers to contested/evidence-required/abstain states when replacement evidence is insufficient?

**V1 basis:** EP04 expected ABSTAIN. G2 produced open/contested H2, while V1 selective coverage counted any non-null open hypothesis as covered.

**Required V2 representation:** separate operative hypothesis from committed answer and score calibration explicitly.

## H7 — Refutation response semantics

**Question:** What constitutes a valid response to decisive refutation: reopen, hypothesis change, commitment downgrade, abstention, or some combination?

**V1 basis:** EP02 received response credit for a reopen that stayed on H1; EP01/EP03 received no response credit despite prompt H1→H2 cross-step changes.

**Requirement:** preregister response semantics before V2 scoring.

## H8 — Cost/value tradeoff

**Question:** What additional runtime/search cost is attributable to HypoKosh and DWM, and is any measured epistemic gain large enough to justify it?

**V1 basis:** G0/G1/G2 share the compiled runtime path, making their relative execution-cost deltas more interpretable than B0→G0.

## Freeze rule for later V2

No score-bearing V2 execution may begin until:

- task families are expanded beyond the four V1 episodes;
- capability contracts are explicit;
- common vs capability-specific metrics are declared;
- cross-step and within-call transition semantics are separated;
- diagnostic telemetry is frozen;
- evaluator code and task manifests are hashed;
- score thresholds/effect criteria are preregistered without inspecting score-bearing V2 outputs.
