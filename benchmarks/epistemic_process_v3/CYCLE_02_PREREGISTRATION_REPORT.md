# EP-PROCESS-V3 — Cycle 2 Preregistration Report

Status: **PENDING CI VALIDATION**

## Objective

Freeze V3 semantics, hypotheses, controls, task-family requirements, telemetry, metrics, coverage-aware safety rules, numerical claim thresholds, statistics and failure policy before any V3 production fix or final score-task generation.

## Files Created

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
- `claim_gate_v3.py`
- `validate_preregistration_v3.py`
- `test_metric_semantics_v3.py`
- `test_claim_gates_v3.py`

## Semantic Decisions

The semantic contract freezes:

- identity-invariant tie behavior;
- explicit distinction between refutation, revocation, supersession and invalidation;
- contested/null-operative handling for the refuted-incumbent/under-corroborated-replacement case;
- explicit open/contested/abstention separation;
- native revision requirements;
- earned-resolution requirements;
- challenge vs corroboration-search separation;
- production-observable DWM expansion opportunity;
- meaningful reopen usefulness.

## Hypotheses Frozen

Twelve V3 hypotheses are linked to explicit claim families in `measurement_model_v3.json`.

## Controls

Primary score profiles remain C0/G0E/C1/G1/G2. No extra score-bearing ablation is authorized.

## Task-Family Requirements

Cycle 3 must generate 384 episodes: 24 families × 16 episodes, with 8/8 H1/H2 mirroring on hypothesis-sensitive families.

No final V3 tasks are created in Cycle 2.

## Runtime/Oracle Boundary

Runtime-forbidden evaluator/oracle fields are frozen. Oracle leakage globally invalidates a future run.

## Telemetry Contract

Target-level, evidence-state, native-transition and DWM opportunity telemetry requirements are frozen. Canonical family identity is `family:<raw-id>`.

## Metrics

Metric numerators, denominators, eligibility, missing-data treatment, runtime-failure treatment and direction are frozen.

## Coverage/Safety Framework

No weighted composite is used for primary claims.

Broad DWM outcome requires a safety improvement while preserving commitment coverage and correct-commitment yield within five percentage points, with operative and committed accuracy non-inferiority margins also frozen.

## Claim Gates

Nine claim families are frozen.

## Numerical Thresholds

All thresholds are contained in `claim_gates_v3.json` and mirrored in `measurement_model_v3.json`.

## Statistical Plan

Exact McNemar, paired bootstrap 95% CI, Holm correction, seed `20261002`, 20,000 bootstrap resamples.

## Failure Policy

One untouched score run, no retry, no imputation, global invalidation rules and claim-unscorable rules are frozen.

## Adversarial Validation

Pending Cycle-2 CI execution.

Synthetic tests cover:

- always-abstain;
- always-answer unsafe;
- safety gain with coverage collapse;
- overactive/no-op DWM;
- node-ID-biased tie;
- symmetric abstention;
- fabricated revision;
- false earned resolution.

## Unresolved Questions

None of the 20 required semantic questions remain unspecified.

Implementation details for satisfying the frozen semantics are intentionally unresolved and belong to later cycles.

## Repository State

- branch: `lab/epistemic-process-v3-cycle2-preregistration`
- parent Cycle-1 head: `2b6d98c1d4135e6a6e482832c69acc8091bb7fc5`
- commit/tree/PR/CI/artifact: to be filled after exact Cycle-2 validation

## Gate

**INCOMPLETE — awaiting Cycle-2 CI validation**

## Next Authorized Activity

If and only if Cycle 2 passes: **V3 Cycle 3 — task-universe construction**.
