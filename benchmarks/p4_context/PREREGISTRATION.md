# P4 — Frozen Research Protocol: Native Contextual Learning Across Weather and Financial Time Series

Experiment: `MOOL-P4-CONTEXT-001`. Status: **PREREGISTERED / PRE-SOURCE-INSPECTION**.
Frozen 2026-10-18, before reading contents or evaluating financial dataset `stocks.csv`. P3 results are known and are preserved as prior evidence; the P4 weather design intentionally tests *implementation parity*, not a novel blind weather success. No changes to EP-PROCESS V1/V2/V3 frozen assets or P3 result gates.

## Central falsifiable proposition

Can a **native MoolBase context-aware predictor** train incrementally and persistently on sequential, lagged external observations, preserving prior forecasts and source attribution, without leaking future information—and improve out-of-sample Brier error over a lifetime Beta baseline in **both** an environmental and a financial domain? A two-domain advantage is NOT assumed and can fail. This is supervised online learning, NOT causal inference, autonomous discovery, or neural LLM training.

## Data frozen by external version and observation task

**Weather:** Vega/vega-datasets commit `175a81ca00b2a0d9bd1159ddcf2b7b232e591d93`, file `data/weather.csv`, same vendored file/hash as P3. Seattle and New York Jan 2012–Dec 2015. Event y=1 if precipitation>0. Information available at forecast: previous day's observed precipitation, max/min temperature and wind, plus forecast-date day-of-year. Ignore categorical `weather` column. Weather tests: 2012–2013 warmup, 2014 dev, 2015 chronological test. Source dates sorted and complete as in P3. Native contextual model MUST match the P3 Python lagged-logistic model to within 1e-9 for every row (cross-implementation parity).

**Finance:** Same Vega/vega-datasets commit, file `data/stocks.csv`, `symbol,date,price` columns (if actual schema differs, STOP and mark design mismatch rather than silently changing after data inspection). Each symbol has its own independent learner. Outcome y_t=1 iff price_t>price_{t-1}; when equal it is 0. Before price_t arrives, use only price up to t-1. Feature vector:
`[1, clamp(r_{t-1},-1,1), clamp(r_{t-2},-1,1), mean(last_3_clamped_returns), stddev_population(last_3_clamped_returns), indicator(r_{t-1}>0), sin(2*pi*month_t/12), cos(2*pi*month_t/12)]` where `r_j=price_j/price_{j-1}-1`. Require >=4 previous prices before first forecast. Missing-price records, duplicate symbol/date, nonpositive prices, malformed dates or unsorted per-symbol chronology invalidate experiment; no imputation. Treat unequal irregular intervals as sequential consecutive *observations* and disclose lack of trading-day normalisation. For each symbol with >=50 evaluable predictions, chronological first 60% warmup, next 20% dev and last 20% test; split counts determined by floor(0.6n), floor(0.8n) over all evaluable predictions (never chosen based on outcomes). If any symbol has <50 evaluable records, exclude that symbol from both model arms, list exclusion and rationale; if <3 eligible symbols experiment is INELIGIBLE_DESIGN. Source shall be vendored after preregistration, with exact SHA-256 reported. This is public historical data, not independent financial ground truth certification.

## Fixed native algorithm and comparison policies

**P4 native context learner:** Online logistic regression with initial weights all zero, fixed learning rate `0.12` and fixed non-intercept L2 coefficient `0.0005`; `logit_t=sum(w_j*x_j)`, `p_t=sigmoid(clip(logit_t,-15,15))`. Pre-commit p_t **and the exact features plus feature provenance date** as Proposed/Hypothetical node before reading y_t. When separately caller-attested y_t arrives with nonzero evidence ID, distinct evidence family and nonblank verifier ID, append Observed/Verified outcome linked to forecast. Then update `w_j <- w_j - 0.12*((p_t-y_t)*x_j + (j>0 ? 0.0005*w_j : 0))`. On restart, reconstruct weights from valid historic forecast→outcome records; reject duplicated trials/families, mismatched probabilities, retroactive/forged observations. Never write a predictive hypothesis as Verified. Features and evidence IDs are caller assertions: authentication is outside scope.

**P1 native control:** Frozen lifetime Beta(1,1) in MoolBase. The P3 full-history approach remains unchanged except possible audited performance optimisation with identical forecasts. No training data may come from the future.

**Weather additional benchmarks:** P3 Rolling14 and P2 unchanged remain exploratory historical comparators. **Finance additional control:** Rolling4 Beta(1,1) over last four completed *outcomes* per ticker. Both domains report Brier and log loss; native contextual and native P1 must be executed on identical chronological data.

## Claims and measurement

For weather, report two-city pooled 2015 P4 vs P1 difference, not a novel blind discovery. Weather implementation-parity gate: all 2,922 native contextual predictions match P3 Python lagged-logistic baseline (1e-9) and maintain original 2015 scores to displayed 6 digits.

For finance, **PRIMARY NEW P4 SCIENTIFIC GATE**: pooled final 20% per-symbol test P1 Brier minus P4 Brier >= 0.01, paired 14-observation circular block-bootstrap 95% CI lower bound >0, and no symbol's P4 regression >0.03. Use 5,000 bootstrap replicates, `random.Random(20261018)`; resample separately within each symbol preserving its test length, then pool all observations. If any condition fails, `NOT_EARNED`; no tuning and no retries. Finance is not hidden blind data (public known dataset), and bootstrap uncertainty has few clusters; it is a modest falsification test, not proof of transferable intelligence.

**Two-domain extension claim** only if finance primary gate is EARNED and P4 weather pooled test Brier beats P1 by >=0.01. Otherwise `NOT_EARNED` (or `INELIGIBLE_DESIGN` for source/experiment integrity failure). Report per-symbol and per-city counts, all negative/null results and 10-bin descriptive ECE. Missing execution / mismatched dataset hash / data mismatch / leaked same-day features / loss of forecast chronology invalidate results, not a negative performance outcome.

## Replay and adversarial conditions

- Duplicate or correlated evidence family cannot increase learning state.
- Prediction event must precede observation, with unique trial and single pending forecast per event.
- Weights inferred on replay match original at restart; a forged forecast probability or feature mismatch cannot override learned state.
- Explicitly reject nonfinite or unbounded features and nonfinite probability.
- Distinct world_id and event_key models do not leak.
- Changing the *current-day* y/weather/prices cannot alter the feature snapshot forecast from yesterday.
- CI runner exports full per-event native predictions, source checksums, exact git SHA, evaluation JSON, native/replay regression tests and confidence interval. Preserve negative results.
- Do NOT reuse P3 source label `weather` as an input, nor 2015 finance labels in training before their individual forecasts.

## Scope and research interpretation

These are contextual *associations*, not learned causal world transitions. An event forecast trained via SGD is a first neural-style learner (single logistic unit), not a language model, world simulation or evidence of autonomous scientific discovery. Follow-up P5 should challenge cross-domain transferable representations on prospectively embargoed tasks with independent data provenance. Changes to this protocol require a new experiment ID.
