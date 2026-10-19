# MOOL-POLYMATH-F10-001 — Evidence-to-opposition forensic and outcome-scoring gate

**Preregistered 19 October 2026 before F10 implementation or result inspection.** Base v9 exact-head `b1c2e0487e3ad01609d111ebe931f81808376af9`. Preserve F1–F9, EP-PROCESS and the v9 7B model scores byte-for-byte.

## Falsifiable diagnosis
The v9 native opponent was actually invoked 3/3 times but emitted **zero** material challenges/reopen requests. `EpistemicController::oppose()` only considers correlation-group representatives with `FiberPathRole::Opposition` AND `eligible_for_opposition=true`; an unverified caller-created `EdgeRole::Contradicts` may fail graph-retrieval, path-validity, provenance, independence or semantic-opposition criteria. Diagnose *which* stage rejects each raw edge before changing policies or code.

## Fixed study and actual native components
Reuse identical v9 `gate2_native_opposition.cpp` synthetic three-snapshot graph (baseline, contradiction, revision) and same G0/G1/G2 runtime settings, with F10 instrumentation ONLY. Three authored hypotheses:
- H-A warehouse receipt-posting delay
- H-B stale asynchronous stock snapshot
- H-C wrong replenishment policy.

All 3x3 runtime ablations must execute against actual `GrapheneDB` and `CompleteHypoKoshRuntime` without mutating existing epistemic-control logic. Every input source and edge remains `EdgeOrigin::Hypothetical`, `source_id=synthetic-design-only`, `evidence_status=unverified`; never label it Observed/Verified.

Add read-only telemetry at:
1. persisted GrapheneDB outgoing edges by type and origin;
2. retrieved `FiberBundle` roots and total paths;
3. support, opposition, noise and unknown path roles; `eligible_for_support` / `eligible_for_opposition`; nonoperative/hypothetical markers and correlation-group representative eligibility;
4. material `EpistemicAdmissibility` and `OppositionReport` coordinates and challenge/reopen outcomes;
5. selected answer/status, no ModelWorld mutation and no fabricated external verification.

The **separate** scenario design file records `synthetic_designer_preferred_hypothesis` by stage (baseline H-A, contradiction H-B, revision H-B), fixed before execution. These are **not adjudicated empirical truths**, have *zero* evidentiary standing and MUST NOT be supplied to runtime. Score correct-cause coverage vs this transparent designer preference solely as an illustration, and score 'supported truthful outcome' as **NOT_MEASURABLE** rather than 0/100 or fake 100. For each arm/stage, record `has_operative_selection`, `preferred_hypothesis_matched`, selected index/status, unsafe 'Verified' promotions (must be zero), missing-evidence abstentions, number of admitted challenge-eligible representatives and pre/post updates.

## Frozen acceptance
- Engineering G0: 9/9 native ablations and 9/9 diagnostic receipts, exact hashes, negative source/metadata tests.
- G1: every raw contradiction edge is accounted for by persisted edge counts and downstream disposition; explicitly report if it was not retrieved, was not categorized Opposition, or was ineligible.
- G2: report whether any eligible opposition representative reaches DWM, whether actual challenge/reopen occurs and whether primary hypothesis changes.
- G3: no positive scientific accuracy or model superiority claim without independently sourced observations and external adjudication. No thresholds tuned to rescue the v9 result; zero/null accepted.
- Runtime selection compared against designer labels only descriptively; provenance and safety outweigh arbitrary synthetic oracle ranking.

## Next experiment required for genuine revision
Acquire licensed and independently adjudicated incidents with time-ordered raw observations, signed/validated provenance and clear source-family independence. Add an audited adapter that maps each observation to `supports`, `contradicts`, `not_applicable` with span references and *separate* external-verification callbacks. Partition cases and freeze ground truth before run. Compare true prediction accuracy, decommit/recommit rates, false commitment, coverage, cost, latency and abstention. Without these data, a new graph-only synthetic result is infrastructure forensics, not independent scientific proof.
