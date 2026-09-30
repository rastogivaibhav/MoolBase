# Epistemic Process V2 Protocol

Status: **PREREGISTERED / UNSCORED**

V2 exists because V1 successfully froze the experiment but exposed two measurement-boundary problems: G0 was compared on a terminal-answer metric despite intentionally having no hypothesis selector, and cross-step belief changes were not represented as native Revision events.

## Measurement separation

### G0

Primary claims concern evidence/provenance integrity, not terminal-answer superiority.

### G1

Measure selection quality, temporal hypothesis change after newly arriving evidence, abstention, false resolution, and cost.

### G2

Measure everything in G1 plus challenge generation, reopen effectiveness, answer-change conversion, unnecessary churn, and cost.

## Revision terminology

- **Temporal hypothesis change**: the selected hypothesis differs between consecutive observation steps.
- **Native Revision**: a production runtime `EpistemicEventType::Revision` event.
- **Refutation response**: a preregistered response to decisive contradictory/revocation evidence.
- These are reported separately.

## Abstention

An explicit `abstain` is a deliberate terminal semantic state. `open` is unresolved processing. Neither may be silently rewritten as the other.

## Score lock

All V2 tooling remains unscored until a future immutable score manifest records exact source/tree/task/evaluator hashes, policies and allowed post-candidate changes.
