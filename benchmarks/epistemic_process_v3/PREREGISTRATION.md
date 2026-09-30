# EP-PROCESS-V3 — Frozen Preregistration

Status: **FROZEN_PRE_IMPLEMENTATION / UNSCORED**

Experiment candidate: `EP-PROCESS-V3-SCORE-001`

Cycle 2 freezes the scientific rules under which future V3 task construction, implementation, evaluation and scoring will be judged. It does not implement production fixes, generate final score tasks, or authorize scoring.

## Source evidence

This preregistration is grounded in:

- frozen V2 score run `36744340611`;
- V2 artifact `11111687583`;
- V3 Cycle-1 forensic workflow `36748279512`;
- Cycle-1 artifact `11113966063`;
- `CYCLE_01_FORENSIC_ROOT_CAUSE_REPORT.md`;
- `PREREGISTRATION_PROPOSAL_V3.md`.

V2 remains immutable.

## Semantic decisions

### Tie

Epistemically equal targets are compared using combined absolute/relative tolerance:

- absolute tolerance: `1e-9`;
- relative tolerance: `1e-6`.

Integer coordinates such as independent-family count require exact equality.

Target node ID, target insertion order, evidence insertion order, hypothesis label, lexical order and family name may not break a semantic tie.

An unresolved exact tie emits:

- operative hypothesis: null;
- committed answer: null;
- state: `open` when no material defeated-incumbent condition exists;
- state: `contested` when material opposition/defeat exists.

### Evidence semantics

Refutation, revocation, supersession and invalidation are distinct.

- **Refutation:** counter-evidence. Existing support remains valid positive evidence unless separately invalidated, superseded or revoked. Active contradiction may block commitment/resolution.
- **Revocation:** previously accepted evidence is withdrawn from active reasoning. Audit retention is allowed, active contribution is forbidden.
- **Supersession:** evidence remains historically valid but is replaced for operative reasoning by a newer item. It becomes audit-only/non-operative.
- **Invalidation:** evidence is judged unusable for current reasoning. It remains auditable but contributes neither support nor opposition.

### Incumbent/replacement rule

Frozen policy:

`CONTESTED_CLEAR_INCUMBENT_UNTIL_REPLACEMENT_EARNED`

If:

1. an incumbent is materially refuted;
2. a different target becomes the semantic/ranking leader;
3. the replacement lacks required independent corroboration;

then:

- operative hypothesis = null;
- status = contested;
- committed answer = null.

The old incumbent may not remain operative merely because of historical support strength. The replacement may become operative only after it uniquely leads and satisfies the frozen operative corroboration rule.

### Open, contested and abstention

These are different.

- **Open:** insufficient evidence for a unique operative target without a material defeated-incumbent condition.
- **Contested:** material opposition exists or a formerly operative belief has been defeated without an earned replacement.
- **Abstention:** commitment-layer decision to emit no committed answer.

An operative hypothesis can exist without a committed answer. Abstention therefore does not imply no operative hypothesis.

### Resolution

Fully resolved requires:

- at least two independent support families;
- no active material contradiction;
- semantic verification;
- required stability;
- no unresolved revocation/supersession/invalidation conflict.

`provisionally_resolved` is not `resolved`.

### Revision

Revision requires a native old→new or old→null operative transition caused by new evidence.

Challenge, reopen, rank recalculation or contested status alone are not revisions.

Decommitment and recommitment are recorded separately.

## DWM semantic decisions

### Challenge

A challenge requires actual active material opposition to a current/provisional hypothesis.

Insufficient corroboration alone is not a dialectical challenge.

### Corroboration search

Evidence insufficiency without opposition may request additional corroboration, but the event must be labelled separately from DWM challenge.

### Reopen

A reopen is legitimate only when a runtime-observable expansion opportunity exists.

Positive opportunity classes:

- `NEW_ELIGIBLE_NODE_AVAILABLE`
- `NEW_ADMISSIBLE_PATH_AVAILABLE`
- `BUDGET_BOUND_AND_EXPANDABLE`
- `DEPENDENCY_STATE_CHANGED`
- `EXTERNAL_EVIDENCE_ARRIVED`

Negative classes:

- `FRONTIER_EXHAUSTED`
- `NO_EXPANSION_OPPORTUNITY`

### Useful reopen

A reopen is useful only when it produces meaningful new evidence/frontier state and/or downstream semantic change.

Visited-state growth by itself does not count.

## Hypotheses

Frozen hypotheses:

- H-TIE
- H-PERSIST
- H-REVOCATION
- H-REFUTATION
- H-REPLACEMENT
- H-REVISION
- H-DWM-PRECISION
- H-DWM-OPPORTUNITY
- H-DWM-USEFULNESS
- H-DWM-SAFETY
- H-DWM-COVERAGE
- H-EARNED-RESOLUTION

Exact mappings are in `measurement_model_v3.json`.

## Controls

Score-bearing profiles remain:

- C0
- G0E
- C1
- G1
- G2

No additional score-bearing ablation is preregistered.

Primary causal interpretations remain:

- C0→C1: persistence effect;
- C1→G1: HypoKosh operative/governance effect only;
- G1→G2: DWM incremental effect.

## Task-universe requirement

Cycle 3 must construct exactly **384 episodes**:

- 24 structural families;
- 16 episodes per family;
- 8/8 H1/H2 mirror balance for hypothesis-sensitive families.

Cycle 2 does not generate those final tasks.

The required families are frozen in `task_family_requirements_v3.json`.

## Runtime/oracle boundary

Runtime must never receive:

- expected terminal answer;
- correctness labels;
- challenge-warranted labels;
- revision-required labels;
- outcome class;
- expected DWM usefulness;
- future observations;
- evaluator oracle;
- task-family outcome labels;
- evaluator-only independence labels unless that concept is genuinely runtime-visible.

Any oracle leakage invalidates the run.

## Metrics

V3 reports individual safety, usefulness, coverage and correctness surfaces.

There is **no primary weighted composite utility score**.

Critical coverage-aware metrics include:

- commitment coverage;
- correct commitment yield;
- committed accuracy;
- false commitment incidence;
- appropriate abstention;
- inappropriate abstention.

This is designed so always-abstain cannot earn broad outcome value.

## Frozen thresholds

| Threshold | Value |
|---|---:|
| Tie absolute tolerance | 1e-9 |
| Tie relative tolerance | 1e-6 |
| Tie invariance minimum | 100% |
| Exact-tie unique-selection maximum | 0% |
| Exact-tie abstention minimum | 100% |
| Near-tie correct selection minimum | 95% |
| Persistence history-dependent gain | +5 pp |
| Single-step persistence regression maximum | 3 pp |
| Evidence mechanical exactness minimum | 99% |
| Cross-episode leakage maximum | 0 |
| HypoKosh clear-case regression maximum | 3 pp |
| Replacement-policy conformance minimum | 95% |
| Native revision minimum | 80% |
| False revision maximum | 5% |
| Revision latency p95 maximum | 2 steps |
| Challenge precision minimum | 85% |
| Challenge recall minimum | 70% |
| Unnecessary challenge maximum | 15% |
| Unnecessary reopen maximum | 10% |
| Reopen opportunity precision minimum | 90% |
| Exhausted-frontier reopen maximum | 5% |
| Useful reopen minimum | 60% |
| Evidence discovery after latent-evidence reopen minimum | 50% |
| False-commitment incidence reduction minimum | 5 pp |
| G2 false-commitment incidence maximum | 5% |
| Coverage regression maximum for broad DWM claim | 5 pp |
| Correct-yield regression maximum for broad DWM claim | 5 pp |
| Operative-accuracy regression maximum | 3 pp |
| Committed-accuracy regression maximum | 3 pp |
| Earned resolution minimum | 70% |
| False convergence maximum | 5% |
| Adjusted alpha | 0.05 |

These values are frozen before V3 implementation or score outcomes.

## Statistics

- independent unit: episode;
- paired binary test: two-sided exact McNemar;
- paired interval: paired percentile bootstrap 95% CI;
- bootstrap seed: `20261002`;
- bootstrap resamples: `20000`;
- multiplicity: Holm within preregistered primary claim family;
- zero discordant pairs: p=1, delta=0;
- counts, denominators and effect sizes always reported.

## Sample-size plan

Planned total: 384 episodes.

Each structural family receives 16 episodes. Hypothesis-sensitive families use balanced 8/8 mirrors.

Primary claims pool only their preregistered families and compare the same episodes across profiles. The paired structure is intended to detect roughly 5–10 percentage-point causal differences without relying on post-hoc family selection.

Family-level results remain mandatory even when the primary claim pools multiple families.

Post-hoc power analysis after scoring is forbidden.

## Failure policy

- no automatic retry;
- one score run;
- no imputation;
- failures and negative results preserved;
- oracle leakage, tree/hash mismatch, wrong seed, evaluator/statistics mismatch, missing/duplicate/extra task-config pairs, cross-episode leakage or interrupted score workflow invalidate the experiment;
- required telemetry/receipt failure makes affected claims unscorable;
- runtime crash/timeout is preserved and never retried;
- >1% runtime failure makes the experiment operationally inconclusive.

## Answers to the 20 required Cycle-2 questions

1. **What happens under an epistemic tie?** No unique operative target; open or contested according to material-opposition history; no commitment.
2. **What tolerance defines a tie?** Combined absolute `1e-9` / relative `1e-6`.
3. **Refuted vs revoked vs superseded vs invalidated?** Counter-evidence vs withdrawal vs replaced operative relevance vs unusable evidence; all are distinct.
4. **Does refuted evidence remain positive support?** Yes, unless separately invalidated/superseded/revoked; active refutation adds opposition and may block commitment.
5. **Refuted incumbent + under-corroborated replacement?** Clear the operative target; contested; no commitment.
6. **Can contested carry an operative hypothesis?** Yes in general if it remains unique semantic leader, but not in the frozen incumbent/replacement scenario above.
7. **What is abstention?** A commitment-layer decision to emit no committed answer, distinct from open/contested.
8. **What is earned resolution?** Correct resolved commitment satisfying support, contradiction, verification, stability and validity requirements.
9. **What is revision?** Native Hx→Hy or Hx→null operative transition caused by new evidence.
10. **Legitimate DWM challenge?** Active material opposition, not mere lack of corroboration.
11. **Legitimate DWM reopen?** Challenge/corroboration need plus positive runtime expansion opportunity.
12. **How is expansion opportunity known?** Runtime search state reports unseen eligible nodes/paths, binding-expandable budget, dependency change or external evidence arrival.
13. **Useful reopen?** New admissible evidence/frontier plus meaningful downstream semantic effect; visited-state growth alone is insufficient.
14. **Insufficiency vs opposition?** Separate event classes: corroboration search vs dialectical challenge.
15. **How is always-abstain prevented from winning?** Broad outcome gate requires coverage and correct-yield non-inferiority in addition to safety improvement.
16. **Minimum preserved coverage/yield?** No worse than 5 percentage points versus G1 for broad G1→G2 outcome claim.
17. **Broad DWM claim evidence?** Safety claim earned, coverage/yield non-inferior within 5 pp, operative accuracy within 3 pp, committed accuracy within 3 pp, multiplicity-adjusted significance for primary safety gain.
18. **Which families test these rules?** The 24 frozen structural families in `task_family_requirements_v3.json`.
19. **What oracle data is forbidden at runtime?** Expected answers/correctness, warranted challenge/revision labels, outcomes, future observations, expected usefulness and evaluator-only labels.
20. **What invalidates the experiment?** Any global-integrity failure listed in `failure_policy_v3.json`, including oracle leakage, wrong frozen code/data, incomplete matrix or interrupted score workflow.

## Claim interpretation

Claims are only:

- `EARNED`
- `NOT_EARNED`
- `INELIGIBLE_DESIGN`

No configuration ranking is authorized.

## Score authorization

`score_bearing_authorized = false`

Cycle 2 does not authorize V3 scoring.

## Next activity if Cycle 2 passes

V3 Cycle 3 — construct the 384-episode task universe exactly from the frozen task-family requirements, without changing these semantic or claim rules.
