# EP-PROCESS-V3 Cycle 5 — Native Transition Coverage

Status: **LOCAL MECHANICAL GATE PASSED — REMOTE CI PENDING**

## Objective and Source

Repeat the blind unscored 384-episode campaign on native-transition remediation. Production source is PR #89 at `0b729b6f4c5b78614107c0fa1e97ee7aaf386ff8`; production implementation matches its parent `0a98a880317cfc27ca857c4284e47ef2e3c444b7`. This campaign changes no src/, include/, production tests, frozen candidate, generator, or scientific contracts relative to that base.

The earlier R2 run 36780063690 passed its execution gate but lacked native transition coverage. Its success is retained as historical evidence and is not transferred to this head.

## Corrections and Mechanical Evidence

PR #89's failing test confused refutation alone with the frozen defeated-incumbent/under-corroborated replacement case. The fixture now introduces one stronger replacement support before clearing the incumbent, then independent replacement support before recommitment. It reserves database node zero because transition receipts use zero as null. The focused test also rejects spurious transitions on unchanged evidence and repeated resolution events.

The campaign adapter reserves node zero before mapping targets, preserving insertion-order and relative-ID nuisance variants. The task file and blind input are unchanged. The adapter now supplies PriorEpistemicState from the immediately preceding production result, retaining last commitment through a null gap. G0E receives no prior governed state.

Semantic verification is supplied through the existing production PathVerifier API, using only runtime-visible evidence certificates already present in frozen observations. Every evidence-bearing path edge must carry its certificate; missing/unverified evidence stays unverified. Topology bridges are not treated as evidence certificates. No expected answer, outcome eligibility, or oracle field is read by this verifier.

The hardened validator requires revision, decommitment, recommitment and resolution event coverage separately for G1 and G2. Nine raw-telemetry mutations test rejection of each missing event type and illegal G0E transition activity. These are mechanical coverage requirements, not outcome metrics.

## Local Campaign

Seed: `20261004`. Two passes, each with G0E=384, G1=384, G2=384; 1,152 executions per pass and 2,304 total.

Both passes: zero runtime failures, zero validator-reported telemetry gaps, 3,792 step records and 2,640 recovery rounds. Deterministic comparison passed.

Per-pass native events:

| Event | G1 | G2 |
| --- | ---: | ---: |
| Revision | 256 | 256 |
| Decommitment | 304 | 304 |
| Recommitment | 128 | 80 |
| Resolution | 144 | 144 |
| Challenge | 0 | 288 |
| Corroboration search | 0 | 528 |
| Reopen | 0 | 16 |

G2 recorded latent evidence references in 48 episodes, 16 NEW_ELIGIBLE_NODE_AVAILABLE rounds and 1,312 NO_EXPANSION_OPPORTUNITY rounds. These aggregate observations do not prove correctness, usefulness, or a capability claim.

Native-transition contracts, epistemic controller, HypoKosh runtime, 11 metamorphic contracts, 600 monkey episodes, FiberBundle and compact receipt tests passed locally. All eleven frozen contract/generator/candidate hashes match. Oracle/output key checks passed. No score freeze, score results or claims were created.

## Gate and Next Activity

Local mechanical execution, native event coverage, negative mutations and deterministic repetition: **PASSED**.

Remote dedicated CI and artifact sealing: **PENDING**. Do not claim remote completion before the dedicated run succeeds. Scoring remains unauthorized.

Next activity: run the dedicated CI on this exact branch, seal its commit/tree/run/artifact hashes, then proceed to evaluator-boundary validation with synthetic fixtures only; do not score candidate outcomes.
