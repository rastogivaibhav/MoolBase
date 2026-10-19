# MOOL-EVIDENCE-LEARNING-V13-001 — Executed enterprise-sandbox event streams

**Preregistered before v13 code or scores, 19 October 2026.** Base: fully validated v12 source-trust and synthetic native slice, commit `423e484e4b2e433a97c7a042dfcec5c621b49aa9`. Freeze F1–F12 raw results. No external real ERPNext/WMS credentials have been supplied or authenticated; **this is an instrumented SQLite stand-in, not a genuine ERPNext integration**.

## Research question
Can real native GrapheneDB/HypoKosh/DWM act on events created by *executing* inventory transactions in a source-system sandbox, and does DWM add measurable correct-cause or correction value over an identical native G1 or a deterministic adapter?

## Prospective frozen experiment
Run six internally designed incidents (2 independent numerical variants for each injected condition: ERP receipt posting delay, stale WMS snapshot, and erroneous reorder policy). Each uses two separate transactional SQLite services/stores, ERP and WMS. Execute actual SQL data mutations, queue/snapshot/policy changes, timestamps and audit logs. Export the exact operational event log, including system name, event type, logical time, stock counts, pending-receipt count, snapshot age and reorder rule state. **Do not export the cause name, ground-truth ID, planned correction or repair decision in observations.** Persist hidden intervention and independently calculated repair/outcome records in a separate evaluator directory. Underlying observations come from actual code execution, although scenarios and rules are project-authored and not independently blinded.

At T0 provide only common ambiguous discrepancy observation; at T1 add the diagnostic fields actually present in system state. At T2, execute repair transaction and collect independent stock reconciliation/exception-clearance outcome. **No future events can enter earlier timepoints.** All model/selector arms receive an identical fixed event cutoff and source-family metadata. We must strictly distinguish the fault controller's independently logged *intervention within the test harness* from independent scientific/external adjudication (NOT AVAILABLE).

## Fixed arms
- **R0** transparent handcrafted deterministic event-to-cause rules, score separately. Its cause mapping MUST NOT be hidden in/credited to MoolBase.
- **R1** native G1 actual GrapheneDB + HypoKosh, Theoretical mode because no authentic external source; input graph built only from authorized, declared, label-free event adapter.
- **R2** native G2 identical graph/query/settings + actual DWM opposition.
- **R3** LLM-alone, real pinned Qwen 7B on the same event text without database retrieval, optional separate later job.
- **R4** same LLM with ordinary chronological retrieval from the event log, using same model/token temperature/context controls as R3.

R3/R4 must be scored NOT_RUN until actually executed; do not misrepresent native G0 as LLM-only. For initial native CI only run R0/R1/R2; explicitly label R3/R4 NOT_RUN. An LLM comparison stage can follow without changing this version's native scores, with separate preregistration and model sha receipts.

## Source boundary
Run actual v12 production `GenericRelationIngestor` for raw event assertions, with default `QuarantineUnverified`. All input annotations in native graph `EdgeOrigin::Hypothetical`, independent empirical verification FALSE, source family derived from one common controlled sandbox. A digest of a log file is integrity evidence, **not issuer authentication**. No public LegacyObservedExplicit opt-in is used here. Historical and other direct `put_edge` paths remain an open P0 audit.

## Measurements and required checks
6 incidents × 2 inference timepoints × native G1/G2 = 24 native calls, plus R0 12 deterministic selections. For each: case/time source event counts and hashes, exact cutoffs, raw adapter generated graph relations, native final epistemic status, selected hypothesis, speculative/abstain, actual DWM opposition/challenge, graph restart, latency and computational work. At T2 compute repair reconciliation outcome, but **it must not leak to T0/T1 evaluation**.

For R0/R1/R2 record useful coverage, matches vs injection controller label (descriptive internal accuracy), false commitments, wrong-belief corrections (require actual previous wrong conclusion), abstain-to-proposal separately, appropriate uncertainty and runtime latency. A report with **no advantages / all abstain / errors counts as valid negative scientific evidence**, not successful causal AI.

Failure conditions: later events consumed before cutoff; oracle labels in any inference input; forged `Observed` or `Verified`; mismatch between intervention type and actual SQL repair effects; incomplete case×time×arm matrix; baseline being erroneously omitted in claimed comparison; post-result rule tuning without a new experiment ID.

## Limits
Not actual SAP/ERPNext/WMS, no independent expert test, no external event signatures, no true feedback learning or unseen-case uplift, no production-readiness claim. The fault controller, adapter, and evaluator are all source-visible project code; only their **runtime data flow** is segregated. Later meaningful research requires real or independently built incidents and a complete matched-LLM comparison. Deployment/security P0 remains blocked by broad `put_edge` ingress and public legacy override.
