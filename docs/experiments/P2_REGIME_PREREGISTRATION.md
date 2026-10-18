# P2 Pre-Implementation Protocol — Regime-Aware Single Predictive World

Experiment identity: \`MOOL-P2-REGIME-001\`
Status: **FROZEN DESIGN / NON-SCORE-BEARING SYNTHETIC MECHANISM TEST**
Frozen before the P2 implementation on 2026-10-18. This is a separate P2 research contract and **does not** modify the EP-PROCESS-V1/V2/V3 frozen experiment assets.

## Hypothesis

When independently supported binary outcomes change abruptly, a reversible regime hypothesis based on consecutive pre-outcome surprises will reduce sequential Brier error versus the P1 lifetime Beta–Bernoulli predictor. When data is stationary or contains only an isolated outlier, its prediction should remain identical to P1.

This is a **mechanism and controlled toy experiment**, not evidence of broad generalisation, independently authenticated observations, semantic truth or neural learning.

## Precisely frozen model and baselines

Input is one binary \`event_key\` with strictly sequential trials: record forecast -> externally attested outcome -> next forecast. All forecasts are persisted BEFORE observing their own outcomes, remain Proposed/Hypothetical and cannot be retroactively overwritten. Outcomes require distinct trial, nonzero evidence edge, nonempty verifier ID and previously unused independent evidence-family identity; caller authenticity must be separately verified upstream. Each new regimen is only an *inferred provisional* change, never a Verified fact.

**P1 static baseline:** Beta(1,1) prior, p=(1+total verified positive outcomes)/(2+total verified outcomes). Prequential mean Brier = mean((p_t-y_t)^2).

**P2 adaptive model:** Beta(1,1) within active regime. Before observation, p_t=a/(a+b). An outcome is surprising when y=1 and p_t<=0.25, or y=0 and p_t>=0.75. Consecutive surprises must have the SAME outcome and come from different admissible evidence families. On TWO such surprises in sequence, flag a provisional regime change after the second outcome, set next regime's a=1+2*y and b=1+2*(1-y), and reset surprise streak. Else update a+=y or b+=(1-y). Lifetime P1 baseline always updates regardless. First forecast after a change reflects the new regime; no outcome may alter earlier recorded forecasts. Disallow >1 pending trial per event stream to prevent time ambiguity.

The 0.25 / 0.75 surprise thresholds, 2-surprise trigger and 2-observation seed are **fixed**, not selected against the results of this experiment.

## Frozen synthetic sequences and success gates

Each sequence runs in a fresh ledger, with unique trial identity and independent evidence family. They are deliberately simple development fixtures, not statistical samples or hidden holdouts.

| Case | Sequence | Required changes | Required comparison |
| --- | --- | --- | --- |
| S0 stationarity | 20 zeros | 0 | P2 Brier identical to P1 |
| S1 upwards regime shift | 8 zeros then 12 ones | exactly 1; triggered at the second one | P1 Brier - P2 Brier >= 0.10 |
| S2 downwards regime shift | 8 ones then 12 zeros | exactly 1; triggered at the second zero | P1 Brier - P2 Brier >= 0.10 |
| S3 isolated outlier | 8 zeros, one one, 8 zeros | 0 | P2 Brier identical to P1 |
| S4 alternation | 10 repetitions of zero, one | 0 | P2 Brier identical to P1 |

All Brier scores use the original stored probability at the time of forecast, **not** retroactive forecasts. No claim of statistical significance from five deterministic series is permitted.

## Safeguard tests (non-score-bearing)

- Reject outcome without precommitted prediction.
- Reject second pending prediction in the same event.
- Reject outcome without evidence/verifier/family.
- Reject same-family repeated evidence and same trial even under a forged direct record.
- Reject retroactive outcome->forecast pair or a direct stored forecast with inconsistent recorded probability.
- Do not count an inferred regime-change hypothesis as an outcome or a verified fact.
- Persist/reload the full ledger and reconstruct the exact same posterior, regime count, prequential scores and pending state.
- Keep different world IDs, event keys and evidence families isolated.
- Ensure a false regime alarm cannot be triggered by repeated copies of one evidence source.
- Preserve earlier prediction values after contradictory observations.

## Reporting

Report S0–S4 each with P1/P2 mean Brier, number of detected regime changes, counts and all failure cases. A passing CTest proves mechanism implementation, not autonomous discovery or superiority on external data. Any threshold, sequence or metric change after P2 implementation requires a new protocol ID; no edits to this document during experiment execution. Negative/null results must be reported.

## Next independent research gate

After P2's deterministic mechanism passes, freeze and execute a separate proper **out-of-sample** experiment using independently authenticated observed event streams and embargoed chronological holdout periods. Include data regime shifts not constructed to match detector thresholds; evaluate Brier, log loss, expected calibration error with caution about sample size, false-change rate, adaptation lag, observation delay, verifier compromise, model cost, and statistical uncertainty. Compare P1, P2, a fixed rolling-window predictor and a stationary prior under the same information/compute budgets. No claim of learned causality or autonomous polymath reasoning until such evidence exists.
