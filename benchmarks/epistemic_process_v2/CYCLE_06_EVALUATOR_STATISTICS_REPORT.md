# EP-PROCESS-V2 — Cycle 6 Evaluator & Statistics Freeze Report

Status: **COMPLETE / PRE-SCORE / CANDIDATE OUTCOMES UNOPENED**

## Provenance

- Parent Cycle-5 head: `ec52530406c863bfd954193df065598216cb931e`
- Validated Cycle-6 scientific-code head: `fafe753b45fa9289dbcd3253a5b3f7e3e01ab4d2`
- Cycle-6 evaluator workflow run: `36699461036`
- Cycle-6 audit artifact id: `11088998451`
- Cycle-6 audit artifact digest: `sha256:92bf537d696e8bd7d45b84fd9d81909ee5d9369dc792703af291eaf255c44942`
- Expanded Cycle-5 blind mechanical workflow run: `36699461013`
- Expanded Cycle-5 artifact id: `11088559463`
- Expanded Cycle-5 artifact digest: `sha256:dfcaa0cd88ada9f6fb267be807767faaefc766c0a87266643b68203b38121f67`

## Candidate task universe

The pre-score candidate now contains:

- **270 episodes**
- **9 structural families**
- **30 episodes per family**
- H1/H2 mirrored **15/15 per family**
- score-bearing authorization: **false**

The ninth family is `single_step_control`.

It was added before any candidate outcome was evaluated because the Cycle-3 Graphene persistence claim gate explicitly required that persistent state must not materially regress a single-step control workload. The prior 240-episode universe could not evaluate that gate.

No candidate score was inspected before making this correction.

## Blind mechanical validation of the 270-task universe

The expanded Cycle-5 campaign executed:

- 270 tasks × 5 profiles = **1,350 unscored configuration/episodes**
- **5,700 step records**
- **2,280 recovery-round traces**
- **0 runtime failures**
- **0 telemetry gaps**
- oracle fields present in runtime input: **false**
- oracle join performed: **false**
- outcome metrics computed: **false**
- score-bearing: **false**

Deterministic replay used 18 blinded tasks, two from every family, across all five profiles: 90 executions per replay.

Semantic replay determinism passed.

## Evaluator defects found and corrected before scoring

Cycle 6 found that the original candidate evaluator was too trusting. It did not fail closed on several conditions that could corrupt scientific conclusions.

The frozen evaluator now rejects:

1. duplicate task/config episodes;
2. missing task/config episodes;
3. extra episodes;
4. duplicate configuration declarations;
5. configuration mixing;
6. oracle/evaluator fields leaking into runtime evidence;
7. runtime evidence metadata that differs from the task contract;
8. cross-episode evidence references;
9. missing or reordered observation steps;
10. incorrect ingested evidence ids;
11. committed answers under noncommitting states;
12. fabricated cross-step belief-change events;
13. missing required cross-step belief-change events;
14. fabricated revision events;
15. missing revision events when a recovery round changes operative hypothesis;
16. DWM recovery rounds without matching challenge/reopen events.

All adversarial cases are synthetic. No candidate runtime outputs are used by these tests.

## Frozen metric denominators

Cycle 6 fixes all major metric denominators before scoring.

Important definitions include:

- operative-hypothesis accuracy: correct terminal operative hypotheses / all answer-producing episodes;
- committed coverage: non-null committed answers / all G1/G2 episodes;
- committed accuracy: correct commitments / all non-null commitments;
- false commitment: incorrect commitments / all non-null commitments;
- appropriate abstention: correct null commitments / oracle-abstention episodes;
- refutation response: accepted cross-step change or explicit decommitment / preregistered response-window episodes;
- false convergence: invalid resolved decisions / all resolved decisions;
- earned resolution: correct earned resolutions / structurally resolution-eligible episodes;
- DWM challenge precision: warranted challenges / all challenges;
- DWM reopen usefulness: reopen rounds changing frontier, rank, operative hypothesis, status or commitment / all DWM reopen rounds.

Cross-episode leakage is not merely scored: **any occurrence invalidates the run before metric computation.**

## Frozen statistical implementation

The statistics layer is implemented without third-party dependencies and was validated against fixed synthetic vectors.

Frozen rules:

- independent unit: episode;
- paired binary test: **two-sided exact McNemar**;
- interval: **paired bootstrap percentile 95% CI** for treatment-minus-control mean;
- bootstrap seed: `20261006`;
- bootstrap resamples: `10000`;
- multiple-testing correction: **Holm within each primary claim family**;
- paired continuous reporting: mean, median, paired mean delta and bootstrap interval;
- numerators and denominators are always preserved.

Statistical computation is deterministic under the frozen seed.

## Causal applicability

### C0 → C1

Status: **VALID PRIMARY CAUSAL COMPARISON**

Purpose: isolate persistent Graphene evidence/provenance while holding the common decision head constant.

The Graphene persistence claim additionally requires the new single-step control family to avoid a regression greater than five percentage points.

### C1 → G1

Status: **ARCHITECTURAL OPERATIVE COMPARISON ONLY**

C1 has no committed-answer capability.

Therefore a causal claim that HypoKosh improves commitment calibration relative to C1 is structurally invalid in this V2 profile set.

Cycle 6 explicitly marks the HypoKosh commitment-calibration claim as:

`INELIGIBLE_DESIGN`

rather than inventing a denominator or adding a post-hoc configuration.

### G1 → G2

Status: **VALID PRIMARY CAUSAL COMPARISON**

Purpose: isolate DWM with Graphene + HypoKosh held constant.

Both process-value and outcome-value claim gates are implemented before score-bearing output exists.

## Claim gates frozen

The pre-score claim gate can emit only:

- `EARNED`
- `NOT_EARNED`
- `INELIGIBLE_DESIGN`

It does not rank configurations.

The DWM outcome gate requires:

- a preregistered outcome improvement of at least 10 percentage points;
- paired CI excluding zero;
- Holm-adjusted p-value <= 0.05;
- no committed-accuracy regression greater than five percentage points.

The DWM process gate separately requires the preregistered challenge precision, unnecessary-reopen, reopen-usefulness and false-commitment noninferiority thresholds.

## Frozen input SHA-256 checksums

- `evaluate_v2_candidate.py`: `7a3eeee3b035e1601a1f0a255563e145a1344a2fe64a2a36c0053049c8c02569`
- `statistics_v2.py`: `b7b296e4908f235702741087922add83457238652b6d264c9399d0c736cb42c7`
- `claim_gate_v2.py`: `314df06b0ccea0bc470804aa5f25675a3f8580ca30632ca6198278ae12ff8f29`
- `metric_definitions_v2.json`: `72ef7d5adb30a2ba08312cf4bcc4565a503bf8cb6d66c7313b706597ea518487`
- `tasks_v2_candidate.json`: `0960f2b0af349bb122a8dc026a94bbd128f9081e6750eeed6ac67ca81f90f2a7`
- `generate_tasks_v2.py`: `0dece1d693c9475e995a15367151dffeb0f12bc48187d395a949441ca9c4a2e1`
- `measurement_model_v2.json`: `def0ac19f6f7a94afe12ca166fb51ede0b2576c276d5ed23728742fa97ffe689`
- `telemetry_contract_v2.json`: `735f1a7fa1ed4c2d27dc24ce1aee5e7c9e30310ab20d41a552d7d7713e1914ab`

## Scientific boundary

At completion of Cycle 6:

- no candidate outcome score has been opened;
- no candidate oracle join has occurred;
- no claim gate has been run on candidate evidence;
- no threshold was selected from observed candidate performance;
- score-bearing authorization remains false.

## Next authorized activity

Cycle 7 may construct the **pre-freeze candidate boundary**:

1. verify exact source/task/evaluator/statistics hashes;
2. run adversarial pre-freeze gates;
3. freeze the candidate commit/tree;
4. create the immutable V2 score authorization manifest;
5. permit only that manifest after the candidate boundary.

The first score-bearing V2 execution must still occur only after that immutable authorization is merged and verified on exact master.
