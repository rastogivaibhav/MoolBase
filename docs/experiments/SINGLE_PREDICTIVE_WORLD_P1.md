# Single Predictive World — Research Baseline P1 (2026-10-18)

## Purpose and non-claims

This isolated experiment tests whether MoolBase can record a prediction **before** an outcome, bind independently attested observations to it, update a bounded probability estimate, and replay the same learning after restart. It is a prerequisite for autonomous world-model research, not a complete DWM population or a causal simulator.

Not claimed: neural weight training, autonomous evidence gathering, verified source authenticity, causal identification, calibration in the wild, cross-domain transfer, agent autonomy, or an improvement over trained external models.

## Why the evaluation must be staged

The untouched score-bearing V2 evidence is in GitHub Actions run 36744340611, artifact 11111687583, experiment EP-PROCESS-V2-SCORE-001. Direct measurement across 270 paired episodes:

| Frozen V2 endpoint | G1 HypoKosh | G2 HypoKosh + DWM |
| --- | ---: | ---: |
| False commitments (count) | 30 | 0 |
| Commitment coverage | 240/270 | 30/270 |
| Correct commitments (count) | 210 | 30 |
| Correct operative hypotheses (count) | 210 | 210 |
| DWM challenge precision | n/a | 570/1020 |
| DWM useful reopens | n/a | 0/1020 |

The frozen evaluator says DWM outcome claim EARNED, DWM process claim NOT_EARNED. This is not an overall DWM victory: prevention of false commitments also prevented many correct commitments. Do not infer new semantic correctness, use selection-only accuracy, or claim autonomous learning.

The V3 claim-gate file marks score_bearing_authorized=false. Its 384-episode campaign is UNSCORED. Do not unfreeze it by accident or compute an outcome scoreboard against its future oracle.

## Model and update rule

This P1 experiment holds **one world_id** with named binary event hypotheses, independently precommitted trials, and verified observed outcomes. For event e, the provisional predictive state is:

    p_e = (1 + number_of_true_verified_outcomes)
        / (2 + number_of_verified_outcomes)

Equivalent to a Beta(1,1) prior / Bernoulli posterior predictive mean. This models an event frequency, **not** a counterfactual transition or learned causal graph. The binary Brier score for a forecast p made before observed outcome y is (p-y)^2; lower is better. This is prequential (predict, then observe, then update) and is distinct from a blinded held-out population experiment.

Every forecast is saved as a Proposed/Hypothetical ModelWorld Hypothesis node with its original immutable probability and trial key. A caller-attested outcome becomes an Observed/Verified Outcome node only when it has a prior forecast, nonzero evidence-edge reference, verifier ID, and distinct evidence-family ID. Duplicate trial/outcome and same-family copied observations fail closed. The persistent ModelWorld save/load contains all forecast/outcome records; estimates are reconstructed by replay rather than by caching a mutable number. The system must not silently promote the forecast node to Verified.

**Trust boundary:** existence of an edge ID and a verifier ID is only caller-supplied metadata. This implementation cannot independently authenticate an external measurement; production must bind receipts to authenticated verification authorities, source integrity and immutable evidence objects. A corrupt-but-well-formed externally injected metadata node remains a broader threat.

## Reproduce

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON
    cmake --build build --parallel 2 --target graphenedb_predictive_world_tests
    ctest --test-dir build -R graphenedb_predictive_world_tests --output-on-failure
    ctest --test-dir build --output-on-failure

The deterministic native toy test uses six independent negative outcomes, then a contradicting positive outcome. It checks the Beta updates, Brier score versus a constant 0.5 forecast, duplicate family rejection, no premature observation, refusal of blank verifier and evidence IDs, immutable initial forecasts, persistence/replay, and independent world namespaces.

Passing this small toy experiment establishes only that the **implementation mechanism** behaves as specified. It provides no empirical evidence that it beats G1, G2, or any external reasoning model on real data. It is intentionally separate from the frozen V2/V3 scientific protocols.

## Next scientific gates

1. Authenticate observation evidence and verifier authority; prevent forged positive/negative labels.
2. Build an externally adjudicated, pre-registered time-series dataset with immutable training/validation/test splits and at least one shift in event probabilities.
3. Compare Brier, log loss, calibration/coverage and proper abstention with Beta base-rate, persistence-only, fixed LLM, and retrieval-based baselines.
4. Test change-point adaptation, source-dependency attacks, measurement lag, manipulation, and catastrophic forgetting.
5. Define predictive transition operators before proposing a population of separately evolving causal worlds.
6. Only then trial scheduled offline replay, cross-domain hypotheses, and outcome-driven trainable selection policies.

Do not alter any frozen EP-PROCESS scoring or claim-evidence file in this branch.
