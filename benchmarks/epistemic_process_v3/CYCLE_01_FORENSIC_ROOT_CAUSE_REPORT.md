# EP-PROCESS-V3 — Cycle 1 Forensic Root-Cause Report

Status: **DIAGNOSTIC COMPLETE / NO PRODUCTION BEHAVIOR CHANGED**

## Objective

Identify the exact mechanisms behind the V2 findings before implementing any V3 behavior change.

Priority order:

1. HypoKosh H1 tie bias.
2. Graphene stale-context / persistence influence.
3. DWM inert challenge/reopen chain.
4. G0E family-namespace measurement defect.

V2 remains immutable historical evidence. No V2 task, score, evaluator, artifact, threshold, or runtime behavior was changed or rerun.

## Repository state

Diagnostic branch:

`lab/epistemic-process-v3-cycle1-forensics`

Parent frozen V2 master:

`723ac328458411e43680a2853d31d30006fc18a2`

Cycle-1 workflow:

`EP Process V3 Cycle 1 forensic diagnostics`

Validated diagnostic head before this report:

`df219207220b27868d7ba95bc5a764c3b11e9262`

Workflow run:

`36748279512`

Artifact:

- id: `11113966063`
- digest: `sha256:9500dd55d8574ebe04905ffe55dd748a0309da9a9c0d78968bd5a81ff083b6f8`

The workflow explicitly verified that the diagnostic branch contained no changes under `src/` or `include/`.

Inherited EpistemicController and CompleteHypoKosh runtime tests remained green.

---

# Finding 1 — HypoKosh tie bias is a numeric target-ID bias

## Frozen V2 observation

All 30 V2 genuine-ambiguity G1 episodes:

- selected H1: 30;
- selected H2: 0;
- abstained: 0;
- H1 node id = 0: 30;
- H2 node id = 1: 30.

The competing targets had equal epistemic coordinates at the terminal tie:

- equal support strength;
- equal opposition strength;
- equal belief strength;
- equal semantic verification;
- equal independent support-family count;
- equal best-support score.

## Responsible production code

`src/epistemic_control.cpp`

The target ranking comparator resolves the final equality with:

`left.fiber->target_node < right.fiber->target_node`

The support-candidate ordering also uses lower target node as a deterministic tie-break, and the support-leader logic preserves lower target node under equal support.

Therefore numeric node identity enters epistemic selection when all meaningful epistemic coordinates are equal.

The V2 runtime inserts H1 before H2, so H1 receives the lower node id.

## Mechanical metamorphic reproduction

The Cycle-1 production-runtime probe produced:

- baseline: H1 node 0, H2 node 1 → **H1 selected**;
- swapped root insertion: H2 node 0, H1 node 1 → **H2 selected**;
- reversed evidence insertion with original node IDs → **H1 selected**;
- renamed evidence families with original node IDs → **H1 selected**;
- eight repeated deterministic runs → same lower-node result.

## Alternative explanations rejected

The evidence rejects these as the primary cause:

- hypothesis label semantics;
- evidence insertion order;
- evidence-family names;
- nondeterminism;
- unequal support coordinates.

## Root-cause classification

**PROVEN**

The V2 "H1 bias" is actually a **lower numeric target-node tie-break leaking into epistemic meaning**.

H1 only wins because H1 happened to receive node id 0.

## V3 implication

V3 must remove incidental storage/insertion identity from semantic tie resolution.

A genuinely symmetric unresolved tie must not be converted into an operative hypothesis solely by numeric node identity.

No fix is implemented in Cycle 1.

---

# Finding 2 — "Stale context" is three different mechanisms, not one Graphene defect

The V2 C0→C1 aggregate result concealed two exact offsetting families:

| Family | C0 correct | C1 correct | Delta |
|---|---:|---:|---:|
| genuine_ambiguity_tie | 0/30 | 30/30 | +30 |
| insufficient_replacement | 30/30 | 0/30 | -30 |
| every other family | same | same | 0 |

This explains the aggregate 0-point C0→C1 effect.

## 2A — Explicit revocation is mechanically correct

V2 demonstrated:

- C1 revocation visibility: 100%;
- evidence retention exactness: 100%;
- cross-episode leakage: 0.

The runtime deletes explicitly revoked evidence nodes before the next projection.

There is no evidence that explicitly revoked evidence leaked into later active reasoning.

**Classification: NOT A REVOCATION LEAK.**

## 2B — Refutation intentionally does not revoke prior support

The common-head forensic contract reconstructs the V2 insufficient-replacement sequence:

1. H1 support family A;
2. H1 support family B;
3. H1 refutation family R;
4. H2 replacement support family C.

C0 sees only event 4 and selects H2:

- H1 = 0;
- H2 = +1.

C1 retains all valid history:

- H1 = +2 support −1 opposition = +1;
- H2 = +1 support;
- result = tie → **open**.

If the two prior H1 supports are explicitly removed from the active set:

- H1 = −1;
- H2 = +1;
- the same decision head selects H2.

Therefore the C1 behavior follows the declared evidence model:

> **refutation is opposition, not revocation.**

This is not evidence of Graphene retaining evidence that the runtime considered revoked.

The V3 design question is semantic: after material refutation, should historical supporting evidence remain active support, remain visible only for audit, or become a separate invalidated/superseded state?

That policy must be preregistered before a production change.

## 2C — HypoKosh's corroboration safeguard can keep the refuted incumbent operative

In every one of the 30 V2 G1 insufficient-replacement episodes:

- the first item in the target ranking was not the final operative target;
- terminal status was contested.

The responsible code path is `select_primary_target()`.

After refutation, the old incumbent may have:

- stronger raw support strength from two historical independent families;
- very low belief strength because opposition attenuates it.

The new replacement may have:

- higher belief strength;
- only one independent support family.

When material opposition makes the replacement the belief leader but the old hypothesis remains the strictly stronger support leader, the corroboration safeguard asks whether the replacement has at least two independent support families.

If not, it returns the old support leader.

Thus V2 can produce:

- ranking leader: replacement hypothesis;
- operative hypothesis: old contradicted incumbent;
- governed status: contested.

## Root-cause classification

**PROVEN POLICY INTERACTION**

The insufficient-replacement behavior is not a simple storage leak.

It is the combination of:

1. persistent support remaining active after refutation;
2. refutation being represented as opposition rather than invalidation;
3. HypoKosh's anti-false-promotion corroboration safeguard preferring a historically stronger incumbent when the replacement has <2 families.

## V3 implication

V3 must explicitly distinguish:

- historically supported;
- currently active support;
- refuted/invalidated support;
- revoked evidence;
- superseded evidence;
- audit-visible but non-operative evidence.

V3 must also preregister the desired behavior for:

> incumbent materially refuted + replacement plausible but insufficiently corroborated.

Candidate policies include abstain/contested-without-operative-selection versus a provisional replacement, but Cycle 1 does not choose the fix.

---

# Finding 3 — DWM's 1,020 no-op reopens have a two-part cause

## Frozen V2 evidence

Across the score-bearing G2 run:

- DWM reopen rounds: 1,020;
- `no_progress_after_expansion`: 1,020;
- unchanged bundle hash: 1,020;
- unchanged visited-state count: 1,020;
- frontier changes: 0;
- target-rank changes: 0;
- operative changes: 0;
- status changes: 0;
- commitment changes: 0.

Reopen-node sizes:

- one node: 420;
- two nodes: 600.

Every DWM round increased one or more search budgets.

Yet actual visited states before reopening were only 4–12 while:

- semantic candidate limit = 32;
- max paths = 128;
- max visited states = 4,096.

The V2 episode database contains only two hypothesis roots plus at most six observation nodes.

Therefore the initial search budget was already far above the size of the synthetic task graph.

## Responsible runtime trigger

`EpistemicController::oppose()` sets `requests_reexpansion` if any of these hold:

- opposition score exceeds threshold;
- evidence is inadmissible;
- selected target lacks sufficient independent support;
- retrieval noise is high.

This expresses an **epistemic need for more evidence**.

It does not test whether the graph contains a plausible **search-space opportunity** capable of supplying new evidence.

## Responsible reopen path

`opposition_options()`:

- adds reopen nodes;
- increases semantic candidates;
- increases max paths;
- increases max paths per root;
- increases max visited states.

`GrapheneEvidenceExpander::expand()` then seeds downstream observations from reopened hypotheses before normal semantic candidates.

But in V2's tiny closed task graphs those downstream observation nodes are already discoverable by the initial semantic search.

The expanded budgets are therefore non-binding.

The reopened bundle is identical.

## Mechanical reproduction

The Cycle-1 runtime probe constructed a single-support G2 case.

It observed:

- opposition search requested: true;
- reopen node supplied: true;
- search options expanded: true;
- bundle unchanged: true;
- visited states unchanged: true;
- stop reason: `no_progress_after_expansion`.

## Root-cause classification

**PROVEN COMBINED RUNTIME + BENCHMARK LIMITATION**

There are two separate causes:

### A. Runtime opportunity problem

DWM asks for re-expansion based on epistemic need without first establishing that reopening can expose a new frontier.

### B. V2 benchmark opportunity problem

The V2 synthetic graphs contain essentially no latent search frontier. The initial search already has enough budget to enumerate the relevant local graph.

Therefore V2 can measure that DWM *requested* intervention, but is poorly constructed to demonstrate useful retrieval expansion.

## Important interpretation

Do not conclude merely:

> "DWM's search algorithm is broken."

The stronger evidence-backed conclusion is:

> DWM currently lacks an expansion-opportunity gate, while the V2 task topology simultaneously supplies almost no hidden frontier for a reopen to discover.

V3 must test both positive and negative opportunity cases.

## V3 required task distinction

### Expansion-available positive controls

A reopen has access to previously undiscovered, admissible evidence that can alter the frontier/ranking.

### Expansion-impossible negative controls

All relevant evidence is already exhausted. DWM should not reopen merely because uncertainty remains.

This distinction did not exist strongly enough in V2.

---

# Finding 4 — DWM conflates "need more evidence" with "dialectical challenge"

A G2 opposition search can be requested simply because:

`!admissibility.sufficient_independent_support`

That condition can exist with no genuine contradictory opponent.

The runtime then emits a DWM `Challenge` event whenever `opposition_search_requested` is true.

This explains why even single-support / single-step cases can produce a dialectical challenge/reopen despite having no new contradictory evidence available.

V2 challenge precision was only approximately 55.88%, with approximately 44.12% unnecessary reopens.

## Root-cause classification

**SUPPORTED BY CODE AND RUNTIME REPRODUCTION**

The DWM event surface currently mixes at least two concepts:

1. **epistemic insufficiency / seek more corroboration**;
2. **dialectical opposition / challenge an existing belief**.

V3 should separate those concepts.

A missing second support family should not automatically be described as a dialectical challenge unless the frozen V3 semantics intentionally define it that way.

---

# Finding 5 — G0E family-identity failure is a schema mismatch

## Frozen V2 evidence

G0E:

- evidence retention was mechanically correct;
- aggregate family-identity exactness = 0%;
- family IDs observed across evaluated step snapshots: 2,640;
- prefixed with `family:`: 2,640;
- unprefixed: 0.

## Responsible production code

`src/fiber_bundle.cpp`

The `families()` function intentionally emits:

`"family:" + evidence.evidence_family_id`

This is a namespace-qualified lineage identifier.

## Evaluator expectation

The V2 evaluator constructs expected family identities directly from task values such as:

`INSUFFICIENT_REPLACEMENT_01_A`

without the `family:` namespace.

Thus:

- production telemetry: `family:INSUFFICIENT_REPLACEMENT_01_A`
- evaluator expected: `INSUFFICIENT_REPLACEMENT_01_A`

## Mechanical reproduction

Cycle-1 G0 probe inserted task family:

`PLAIN_FAMILY`

FiberBundle emitted:

`family:PLAIN_FAMILY`

and did not emit the unprefixed string.

## Root-cause classification

**PROVEN MEASUREMENT-SCHEMA DEFECT**

This does not justify modifying V2.

V3 must freeze one canonical identity representation.

The recommended low-risk approach is for the V3 evaluator to compare task identities in the same explicit namespace used by production telemetry rather than modifying production lineage semantics only to satisfy a benchmark.

---

# Causal failure taxonomy

| Observed V2 problem | Cycle-1 classification | Responsible mechanism |
|---|---|---|
| H1 selected in symmetric ties | Proven implementation bias | lower numeric `target_node` deterministic tie-break |
| C1 insufficient-replacement regression | Proven semantic/policy interaction | retained support + refutation-as-opposition + common-head tie |
| G1 old incumbent remains operative after refutation | Proven policy interaction | `select_primary_target` corroboration safeguard |
| 1,020 inert DWM reopens | Proven combined limitation | no expansion-opportunity gate + saturated V2 graph |
| excessive DWM challenge activity | Supported mechanism | insufficient-support recovery expressed as DWM challenge |
| G0E family identity 0% | Proven measurement defect | `family:` telemetry namespace vs raw evaluator IDs |
| revoked evidence contaminates later state | Not supported | V2 revocation visibility was 100% |

---

# What Cycle 1 does NOT prove

Cycle 1 does not prove:

- the correct replacement for node-ID tie-breaking;
- that all refuted evidence should be deleted;
- that refutation and revocation should mean the same thing;
- that DWM should never reopen;
- that DWM cannot work on a graph containing latent evidence;
- the correct coverage/abstention utility threshold;
- that V3 will outperform V2.

Those are Cycle-2+ experimental-design questions.

---

# Proposed V3 architecture questions

Before production changes, Cycle 2 must freeze answers to these questions.

## Tie semantics

When all meaningful target coordinates are equal:

- abstain?
- contested with no operative target?
- retain prior operative target only if explicitly justified by temporal continuity?

Numeric node identity must not be a semantic criterion.

## Refutation semantics

Does material refutation:

- merely attenuate belief;
- invalidate support for operative selection while retaining it for audit;
- supersede prior evidence;
- require explicit revoke/supersede events?

This must be explicit rather than implicit.

## Incumbent-vs-replacement semantics

When:

- incumbent has strong historical support;
- incumbent is materially refuted;
- replacement is plausible;
- replacement lacks sufficient independent corroboration;

V3 must define whether the correct output is:

- abstain;
- contested/no operative target;
- provisional replacement;
- retained incumbent but noncommitting.

The benchmark oracle must follow the frozen semantic choice.

## DWM opportunity semantics

Before emitting a reopen, the runtime should be able to answer:

`can_reopen_change_search_frontier?`

Negative-control answer `false` should normally suppress a DWM reopen.

## DWM event semantics

Separate:

- seek-more-evidence / corroboration recovery;
- actual opposition challenge;
- reopen;
- revision.

Activity should not be counted as dialectical challenge solely because evidence is incomplete.

---

# Recommended V3 measurement direction

Prefer a **multi-metric / Pareto claim gate** over a single arbitrarily weighted utility number.

At minimum keep separate:

- operative accuracy;
- commitment coverage;
- correct-commitment yield = correct commitments / all eligible episodes;
- committed accuracy;
- false-commitment incidence;
- appropriate abstention;
- inappropriate abstention;
- earned resolution;
- challenge precision;
- unnecessary reopen rate;
- reopen usefulness;
- actual revision rate;
- cost.

A broad DWM outcome claim should require safety improvement **without material collapse in correct-commitment yield/coverage**.

This prevents an always-abstain policy from earning broad superiority.

Exact thresholds remain a Cycle-2 preregistration decision and are not frozen here.

---

# Cycle 1 Gate

**PASSED**

The four requested forensic targets have reproducible mechanisms.

No production behavior was changed.

# Next authorized activity

**V3 Cycle 2 — preregistration only.**

Freeze:

- semantic definitions for tie/refutation/incumbent/replacement states;
- V3 hypotheses;
- causal controls;
- task-family requirements;
- telemetry contract;
- coverage-aware claim gates;
- exact thresholds/statistics/failure policy.

Do not implement the production fixes until that preregistration exists.
