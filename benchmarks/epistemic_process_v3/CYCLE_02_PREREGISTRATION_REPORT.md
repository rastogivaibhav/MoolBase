# EP-PROCESS-V3 — Cycle 2 Preregistration Report

Status: **PASSED / FROZEN_PRE_IMPLEMENTATION / UNSCORED**

## Objective

Freeze V3 semantics, hypotheses, controls, task-family requirements, telemetry, metric denominators, coverage-aware safety rules, numerical thresholds, statistics and failure policy before any V3 production fix or final score-task generation.

## Files Created

Scientific preregistration:

- `PREREGISTRATION.md`
- `semantic_contract_v3.json`
- `measurement_model_v3.json`
- `metric_definitions_v3.json`
- `telemetry_contract_v3.json`
- `claim_gates_v3.json`
- `failure_policy_v3.json`
- `statistics_plan_v3.json`
- `task_family_requirements_v3.json`
- `controls_v3.json`

Validation:

- `claim_gate_v3.py`
- `validate_preregistration_v3.py`
- `test_metric_semantics_v3.py`
- `test_claim_gates_v3.py`
- `.github/workflows/epistemic-process-v3-cycle2-preregistration.yml`

No final V3 score task file was created.

No V3 score workflow was created.

No score-freeze manifest was created.

## Semantic Decisions

### Tie

The semantic tie comparison uses:

- absolute tolerance: `1e-9`;
- relative tolerance: `1e-6`.

Numeric target ID, database/target/evidence insertion order, H1/H2 naming, lexical order and family names are forbidden tie-breakers.

Exact unresolved symmetry emits no unique operative target and no committed answer.

### Refutation / revocation / supersession / invalidation

These meanings are frozen separately.

Refutation adds opposition but does not automatically delete otherwise valid support.

Revoked, superseded or invalidated evidence cannot contribute active positive support.

Audit retention remains permitted/required according to the semantic contract.

### Incumbent / replacement

Frozen policy:

`CONTESTED_CLEAR_INCUMBENT_UNTIL_REPLACEMENT_EARNED`

A materially refuted incumbent may not remain operative solely because it owns historically stronger support when the replacement leads semantically but lacks required corroboration.

That case produces:

- operative hypothesis: null;
- status: contested;
- committed answer: null.

### Open / contested / abstention

The preregistration explicitly treats these as different concepts.

Abstention is a commitment-layer decision and is not silently equated to either open or contested.

### Revision

Revision requires a native old→new or old→null operative transition.

Challenge, reopen, ranking activity and contested state alone do not count as revision.

### Resolution

Fully resolved requires sufficient independent support, no active material contradiction, required semantic verification, stability and no unresolved invalidation-state conflict.

Provisional resolution never counts as fully resolved.

### DWM

Challenge requires actual material opposition.

Evidence insufficiency is a separate corroboration-search condition.

Reopen requires a production-observable expansion opportunity.

Visited-state growth alone does not constitute reopen usefulness.

## Hypotheses Frozen

Twelve V3 hypotheses are frozen:

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

Every hypothesis maps to an explicit frozen claim family.

## Controls

The score-bearing architecture remains:

- C0
- G0E
- C1
- G1
- G2

No additional score-bearing ablation is authorized.

Causal attribution remains:

- C0→C1: persistent-state effect;
- C1→G1: HypoKosh operative/governance effect only;
- G1→G2: incremental DWM effect.

## Task-Family Requirements

Cycle 3 must construct:

- **384 episodes**
- **24 structural families**
- **16 episodes per family**
- **8/8 H1/H2 mirror balance** for hypothesis-sensitive families.

No final task instances were generated in Cycle 2.

## Runtime/Oracle Boundary

Runtime-hidden fields are frozen.

Runtime may not receive expected answer/correctness, warranted challenge/revision labels, expected usefulness, future observations, evaluator oracle, outcome labels or other evaluator-only information.

Oracle leakage globally invalidates a future score run.

## Telemetry Contract

The V3 telemetry contract freezes:

- semantic target-rank coordinates;
- evidence validity/activity/audit state;
- canonical family identity `family:<raw-id>`;
- native transitions;
- DWM trigger class;
- search limits and current usage;
- frontier exhaustion;
- expansion-opportunity class;
- before/after candidate/path/evidence/rank/state changes.

Telemetry is explicitly non-behavior-influencing.

## Metrics

All primary and safety metrics have frozen numerators, denominators, eligibility rules, missing-data handling, runtime-failure handling and improvement direction.

Coverage-aware metrics include:

- commitment coverage;
- correct-commitment yield;
- committed accuracy;
- false-commitment incidence;
- appropriate abstention;
- inappropriate abstention.

## Coverage / Safety Framework

No weighted composite score is used for a primary claim.

A broad DWM outcome claim requires a genuine safety improvement while also satisfying:

- commitment coverage regression no worse than 5 pp;
- correct-commitment-yield regression no worse than 5 pp;
- operative-accuracy regression no worse than 3 pp;
- committed-accuracy regression no worse than 3 pp.

Therefore a V2-style strategy that reduces false commitments by refusing almost everything cannot earn broad outcome value.

## Claim Gates

Frozen claims:

1. `CLAIM-TIE-SYMMETRY`
2. `CLAIM-PERSISTENCE`
3. `CLAIM-INVALIDATION`
4. `CLAIM-HYPOKOSH-SELECTION`
5. `CLAIM-REVISION`
6. `CLAIM-DWM-PRECISION`
7. `CLAIM-DWM-USEFULNESS`
8. `CLAIM-DWM-SAFETY`
9. `CLAIM-DWM-BROAD-OUTCOME`
10. `CLAIM-EARNED-RESOLUTION`

Possible statuses remain:

- `EARNED`
- `NOT_EARNED`
- `INELIGIBLE_DESIGN`

No configuration ranking is authorized.

## Numerical Thresholds

Key frozen thresholds:

- exact tie invariance: 100%;
- exact-tie unique-selection maximum: 0%;
- near-tie correct selection: ≥95%;
- persistence history-dependent gain: ≥5 pp with CI >0 and Holm-adjusted p≤0.05;
- single-step persistence regression: ≤3 pp;
- mechanical evidence exactness: ≥99%;
- cross-episode leakage: 0;
- replacement-policy conformity: ≥95%;
- native revision: ≥80%;
- false revision: ≤5%;
- revision latency p95: ≤2 steps;
- DWM challenge precision: ≥85%;
- challenge recall: ≥70%;
- unnecessary challenge: ≤15%;
- unnecessary reopen: ≤10%;
- reopen-opportunity precision: ≥90%;
- exhausted-frontier reopen: ≤5%;
- useful reopen on positive controls: ≥60%;
- evidence discovery after latent-evidence reopen: ≥50%;
- false-commitment incidence reduction for DWM safety: ≥5 pp;
- G2 false-commitment incidence: ≤5%;
- earned resolution: ≥70%;
- false convergence: ≤5%;
- adjusted alpha: 0.05.

## Statistical Plan

Frozen:

- independent unit: episode;
- exact two-sided McNemar for paired binary outcomes;
- paired percentile bootstrap 95% CI;
- bootstrap seed: `20261002`;
- bootstrap resamples: `20000`;
- Holm correction within each preregistered primary claim family;
- counts, denominators, effect sizes and adjusted/raw significance always reported.

Planned sample size:

- 384 total episodes;
- 24 × 16 family structure;
- paired same-episode configuration comparisons;
- family-level reporting mandatory;
- post-hoc power analysis forbidden.

## Failure Policy

Frozen:

- one untouched score run;
- no automatic retry;
- no imputation;
- failures preserved;
- negative results preserved.

Global invalidators include oracle leakage, frozen-tree/hash mismatch, wrong seed, evaluator/statistics mismatch, missing/duplicate/extra task-config pairs, cross-episode evidence leakage and interrupted score workflow.

Required primary telemetry failures make the affected claim unscorable.

A runtime failure rate above 1% makes the experiment operationally inconclusive.

## Adversarial Validation

Cycle-2 workflow run:

`36753159933`

Result:

**SUCCESS**

The successful gate verified:

- production-behavior changes: none;
- Cycle-1 source evidence identity: passed;
- Python contract compilation: passed;
- preregistration completeness: passed;
- metric semantic tests: passed;
- claim-gate synthetic attacks: passed;
- score authorization remains false.

Synthetic claim-gate cases passed:

1. perfect selective system;
2. always abstain;
3. always answer / unsafe;
4. false-commitment improvement with collapsed coverage;
5. overactive/no-op DWM;
6. node-ID-biased tie;
7. symmetric tie abstention;
8. fabricated revision;
9. false earned resolution.

The first two Cycle-2 CI attempts failed only because the workflow used overly exact text sentinels for Cycle-1 report wording. The guard was corrected without changing any semantic contract, metric, threshold, hypothesis or production behavior.

## Unresolved Questions

No required semantic question remains unspecified.

Implementation details are intentionally unresolved.

Cycle 2 does not select how the production code should implement the frozen behavior; it defines what later implementation must satisfy.

## Repository State

Branch:

`lab/epistemic-process-v3-cycle2-preregistration`

Parent Cycle-1 head:

`2b6d98c1d4135e6a6e482832c69acc8091bb7fc5`

Validated Cycle-2 branch head:

`d5f41522634153900b1f0aac12f8e9cfb90010d0`

Validated tree:

`b911b4e7625a2cb9dac2365649637c236f5a2244`

PR:

`#83`

Workflow run:

`36753159933`

Audit artifact:

- id: `11115781234`
- digest: `sha256:9cbc185d1aca5e5bfb7a624d96e211f32ace11c6b14fcdcd5e35789cbe494ce7`

Artifact audit summary:

- gate: PASSED;
- production behavior changed: false;
- final score tasks created: false;
- score-bearing authorized: false;
- planned episodes: 384;
- families: 24;
- semantic questions answered: 20;
- synthetic claim-gate cases: 9.

### Frozen preregistration SHA-256

- `PREREGISTRATION.md`: `ed7403f47684628785e8d9515d4f2f4f9afbaff93767c6bb7593af685194edd3`
- `semantic_contract_v3.json`: `3873b8a9383782c16fab7c813c5d0d4f969bd424f7fcd1f6818439a05803737f`
- `measurement_model_v3.json`: `799252c703a06b8f9c1a66d697aaa1d965c9edbfb53acc769e37e8759368a5d0`
- `metric_definitions_v3.json`: `3e95ef5b83ffad6a52c88833e750883d5d662ad595e55508e66ddd7348ab1b57`
- `telemetry_contract_v3.json`: `900c186c56689b5c06222839e0d07755dcd81cba9bdf2722f86286347d4cef0a`
- `claim_gates_v3.json`: `4ebf21dd6da6b603be8c5d3a417aaf0f292a17bfd5d298b6effbd5c7c3cb4d31`
- `failure_policy_v3.json`: `e6ab18eaf563bed4fd11ddcd665d118591ef0160c81f7fa064f68261153c74ec`
- `statistics_plan_v3.json`: `bc245acc76a10ecccf540fd3f9951dc865243a07965f6b0fa7949b401b4bd152`
- `task_family_requirements_v3.json`: `c20f9f03e540f945c66cb8b032ee123487c8b47b81cf60b34acbeed879cd1d8a`
- `controls_v3.json`: `102ea72b6cb7caf134d6752890b50e38a1a72c8ad12649a83e7b361d143b9b94`
- `claim_gate_v3.py`: `82399cca8520765dc60f86020860c3347fd39bfd3d3974dae7c90d509a7beccc`
- `validate_preregistration_v3.py`: `1c0425ef55b10b5a07d7374006c8e7e00eb67f1ed8840cb55a0103cba5342612`
- `test_metric_semantics_v3.py`: `9fdf326ec3b5555548a2aa70b59e37b7d4d73165fdfc703dcb747d4571779abb`
- `test_claim_gates_v3.py`: `5b9daec0f34efeac24c45f46a509607f5460f1cc5c22c54547f384e14beee24b`

## Gate

# PASSED

Before implementing any V3 correction, the meanings of tie, refutation, invalidation, abstention, revision, resolution, challenge and reopen are fixed; hypotheses and controls are fixed; task populations are fixed; required telemetry is fixed; metric denominators and safety-vs-coverage rules are fixed; numerical thresholds and statistics are fixed; and synthetic adversarial cases demonstrate that trivial strategies cannot earn the intended broad claims.

## Next Authorized Activity

**V3 Cycle 3 — task-universe construction only.**

Construct the frozen 384-episode universe from the 24 preregistered family requirements.

Do not modify the semantic contract or claim thresholds.

Do not implement production fixes yet.
