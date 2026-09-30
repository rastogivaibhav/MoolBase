# EP-PROCESS-V2 — Cycle 3 Measurement & Telemetry Preregistration

Status: **PREREGISTERED / UNSCORED / IMPLEMENTATION NOT YET AUTHORIZED**

Parent evidence:
- EP-PROCESS-V1-SCORE-001
- frozen V1 score commit `3c008577739b83a94defc47c73275bf8ba76e76c`
- Cycle 1–2 diagnostic head `017ccec22f2acd2062502da00cb23ff16f0589c7`

## Purpose

V2 exists to answer capability questions exposed but not cleanly measurable in V1.

It is not a rerun designed to make MoolBase win.

The primary questions are:

1. Does persistent Graphene evidence/provenance improve decisions that genuinely require history?
2. What does HypoKosh add beyond a deliberately simple common decision head?
3. Does DWM produce useful challenges/reopens, better contradiction handling, or better commitment calibration?
4. When beliefs change, can the runtime show whether the change happened across observation steps or inside a recovery/reopen cycle?
5. What do those capabilities cost?

## V2 profile model

V2 separates historical continuity profiles from causal profiles.

### C0 — stateless common-head control

- sees only the current observation;
- no persistent Graphene state;
- uses the frozen V2 common decision head;
- purpose: causal control for persistence.

### G0E — Graphene evidence-only

- executes Graphene evidence/provenance construction;
- performs no hypothesis selection;
- never receives terminal-answer accuracy as a primary metric;
- purpose: evidence-retention/provenance capability.

This is the capability-equivalent successor to V1 G0; the historical V1 result is not redefined.

### C1 — Graphene + common head

- uses the same common decision head as C0;
- differs from C0 only by receiving Graphene's persistent evidence state;
- purpose: isolate the causal value of persistence/provenance.

### G1 — Graphene + HypoKosh

- Graphene persistent state;
- HypoKosh hypothesis selection/governed projection;
- DWM disabled;
- purpose: measure the effect of the governed selector relative to simpler evidence-state controls.

### G2 — Graphene + HypoKosh + DWM

- identical to G1 except DWM challenge/reopen is enabled;
- purpose: clean causal comparison of DWM.

Historical B0 may be rerun only as a labelled continuity appendix. It is not part of the primary V2 causal chain.

## Common decision head contract

The C0/C1 head is deliberately simple, deterministic and non-oracular.

It may consume only runtime-visible evidence records:
- target hypothesis;
- support vs contradiction role;
- evidence-family identity;
- dependency/derivation identity;
- observed provenance.

It must not consume:
- expected terminal answer;
- task correctness labels;
- `decisive`;
- evaluator-only independence labels;
- future observations.

For each target:

`net_family_score = distinct_support_families - distinct_opposition_families`

Family identity, not raw path count, is the unit of contribution.

The operative hypothesis is the unique target with the greatest strictly-positive net-family score. A tie or non-positive best score yields no operative hypothesis.

The common head emits an **operative hypothesis only**. It does not claim a governed/committed answer. This prevents a deliberately simple control from being mistaken for HypoKosh.

## Operative hypothesis vs committed answer

V2 freezes two separate concepts.

**operative_hypothesis**
: the current leading hypothesis selected by a decision mechanism.

**committed_answer**
: a hypothesis the governed runtime is willing to present as an answer under its epistemic status policy.

An open, contested, speculative, evidence-required, or abstaining state may still have an operative hypothesis while having `committed_answer = null`.

Coverage metrics use committed answers only.

Hypothesis-ranking metrics may use operative hypotheses.

## Transition semantics

V2 defines two distinct transition classes.

### Cross-step belief change

A transition between the final operative state at observation step N-1 and the initial operative state immediately after observation N is ingested.

Examples:
- H1 -> H2
- H1 -> null
- null -> H2

This event must cite the newly ingested evidence id(s).

### Within-call revision

A transition produced inside one `reason()` invocation after a recovery or DWM reopen.

This retains the existing revision concept but is not used as a substitute for cross-step change.

The two event types must never be conflated in scoring.

## Primary task universe

The primary synthetic V2 score set must contain **at least 240 frozen episodes**, balanced across at least eight structural families, with at least 30 episodes per family:

1. duplicate/correlated support;
2. late contradiction/refutation;
3. revocation;
4. insufficient replacement evidence;
5. genuine ambiguity/tie;
6. evidence accumulation over time;
7. correlated-majority vs independent minority;
8. recovery after a previously justified belief becomes stale.

Episode generation, ordering variants and seeds must be frozen before score-bearing execution.

The V1 four episodes may be included as regression fixtures but must not dominate the V2 score.

## Primary metrics

### Persistence / provenance

Applicable primarily to G0E and C1:

- `evidence_retention_exactness`
- `evidence_family_identity_exactness`
- `dependency_lineage_exactness`
- `revocation_visibility_rate`
- `cross_episode_leakage_rate`

### Operative reasoning

Applicable to answer-producing profiles:

- `operative_hypothesis_accuracy`
- `cross_step_refutation_response_rate`
- `cross_step_revision_latency_steps`
- `false_convergence_rate`

### Commitment calibration

Applicable to G1/G2:

- `committed_coverage_rate`
- `committed_accuracy`
- `false_commitment_rate`
- `appropriate_abstention_rate`
- `earned_resolution_rate`

### DWM mechanism

Primary G1→G2 mechanism metrics:

- `challenge_precision`
- `challenge_rate`
- `reopen_usefulness_rate`
- `frontier_change_after_reopen_rate`
- `rank_change_after_reopen_rate`
- `status_change_after_reopen_rate`
- `within_call_revision_rate`
- `unnecessary_reopen_rate`

A reopen is useful if it causes at least one preregistered observable change:
- evidence frontier;
- target rank ordering;
- operative hypothesis;
- governed epistemic status;
- committed answer.

A challenge is considered warranted only when a preregistered structural condition exists, such as material contradiction, competing independently sourced targets, unresolved evidence insufficiency, or a frozen recovery condition. The evaluator owns that label; the runtime never receives it.

### Cost

- execution time;
- visited states;
- expansion rounds;
- evidence edges considered;
- challenge/reopen count;
- additional work per justified correction.

## Primary causal comparisons

### C0 -> C1

Question: what does persistent Graphene evidence state add when the decision rule is held constant?

Primary endpoints:
- history-dependent operative-hypothesis accuracy;
- evidence retention/provenance metrics.

### C1 -> G1

Question: what does HypoKosh governance add over a simple common head?

This comparison is partly architectural rather than a single-feature ablation because the selector changes. Report it transparently; do not attribute every delta solely to one internal submechanism.

### G1 -> G2

Question: what does DWM add when Graphene and HypoKosh are held constant?

This is the primary DWM causal comparison.

## Evidence thresholds

Thresholds are fixed before V2 score-bearing output exists.

### Graphene persistence claim

A persistence claim may be earned only if:
- C1 materially outperforms C0 on history-dependent operative-hypothesis accuracy by at least **10 percentage points**;
- the paired 95% confidence interval for the difference excludes 0;
- evidence-retention exactness is at least **99%**;
- cross-episode leakage is **0**;
- no single-step control family shows a material accuracy regression greater than 5 percentage points.

### HypoKosh governed-selection claim

A governed-selection claim may be earned only if:
- G1 reduces false commitment or improves appropriate abstention / earned resolution relative to simpler controls on the preregistered task classes;
- any claimed rate improvement is at least **10 percentage points** with a paired 95% confidence interval excluding 0;
- no claim is based solely on higher raw coverage.

### DWM challenge/reopen claim

A DWM process-value claim may be earned only if:
- challenge precision is at least **80%**;
- unnecessary reopen rate is at most **20%**;
- reopen usefulness is at least **50%**;
- the G2 false-commitment rate is not worse than G1 by more than **5 percentage points**.

### DWM outcome-improvement claim

A stronger outcome claim requires at least one preregistered outcome endpoint to improve by **10 percentage points** over G1 with paired 95% confidence interval excluding 0, without a greater-than-5-point regression in committed accuracy.

If the process-value gate passes but the outcome gate does not, the allowed claim is limited to process behavior, not better answers.

## Statistics

- The episode is the primary independent unit.
- Binary paired comparisons use exact McNemar tests where appropriate.
- Rate differences must also report paired bootstrap 95% confidence intervals.
- Continuous cost/latency deltas report paired medians, means and bootstrap confidence intervals.
- Every metric reports numerator, denominator and excluded count.
- Primary claim families use Holm correction for multiple testing.
- Effect sizes are reported even when statistical significance is not reached.
- No post-hoc metric may be promoted to a primary claim.

## Failure policy

- No automatic retry of a score-bearing episode.
- Runtime, adapter, timeout and missing-receipt failures are preserved separately.
- No failed episode is imputed as success or silently discarded.
- If any configuration has >1% operational failures, answer-quality comparisons involving that configuration are labelled operationally inconclusive.
- Cross-tenant/cross-episode evidence leakage is a hard invalidation condition.

## Freeze boundary

Before the first V2 score-bearing run:

- task manifest frozen;
- task-generation seeds frozen;
- configuration contracts frozen;
- common-head implementation frozen;
- telemetry schema frozen;
- evaluator frozen;
- statistical analysis frozen;
- failure policy frozen;
- candidate commit and tree recorded;
- allowed post-candidate changes reduced to the score-freeze manifest only.

A V2 score-bearing run is forbidden until an immutable score manifest explicitly authorizes it.

## Anti-optimization rule

No task, metric, threshold, profile or evaluator may be changed after score-bearing V2 outputs are observed merely to improve MoolBase's result.

Any such change creates a new experiment id.
