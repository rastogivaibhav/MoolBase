# P3 Frozen Protocol — Chronological External Weather Forecasting

Experiment: `MOOL-P3-WEATHER-001`
Status: **PREREGISTERED / SOURCE NOT INSPECTED BEFORE THIS COMMIT**
Date: 2026-10-18.
This protocol is independent of frozen EP-PROCESS V1/V2/V3 experiments and makes no changes to their authorization, data or scoring.

## Research question

Does the previously fixed P2 change-point learning rule improve one-day-ahead precipitation-event forecasts beyond the P1 lifetime Beta baseline on a real, externally sourced chronology? If not, do contextual multivariate or rolling-window baselines better explain variation?

## External dataset and provenance

Source repository: `vega/vega-datasets`, git commit `175a81ca00b2a0d9bd1159ddcf2b7b232e591d93`, file `data/weather.csv`.
The dataset contains daily observed weather in Seattle and New York from 2012-01-01 to 2015-12-31, curated/transformed from NOAA material by Vega for illustrative purposes. No observation is cryptographically attested. Data collection quality and transformation are external limitations.
Expected columns `location,date,precipitation,temp_max,temp_min,wind,weather`. **Never use weather**, which is a derived categorical label. Binary outcome: `y_t = 1` iff `precipitation_t > 0`, otherwise 0. Precipitation is observed after the one-day-ahead forecast. Last-day lagged features may be used next day. No weather data from day t enters the prediction of y_t.

Dataset will be snapshotted verbatim to `benchmarks/p3_weather/data/weather.csv` with recorded SHA-256 **after** preregistration, before any evaluation. Reject unexpected columns, missing values, duplicate date/city, non-finite numbers, nonmonotonic or missing calendar days, or fewer than 1461 days per city. Do not silently impute or drop cases. The earliest date per city is feature warmup only for contextual predictions; scored days are 2014 and 2015 and are fully comparable. Raw data is historical public data—not a private, hidden blind benchmark.

## Fixed chronology and information boundary

Each city's days are processed in date order. 2012–2013 is model warmup (no reported score); 2014 is dev/validation *report only*; 2015 is untouched final test. All four models update **prequentially**, i.e. make a prediction before reading the current-day precipitation label, then update after that observation. No tuning on 2014/2015: all hyperparameters are frozen here. Even after reading 2014 labels, no code/model selection changes based on their values. 2015 observations may train future 2015-day predictions only after each day's original forecast has been recorded; this is an online forecasting evaluation rather than static batch holdout.

## Frozen models and controls

Independent model states per location. No source sharing across cities.

1. **P1/native** — Existing `SinglePredictiveWorld`: Beta(1,1) historical Bernoulli mean `(1+sum(y))/(2+n)`.
2. **P2/native** — Existing `RegimePredictiveWorld` unchanged: Beta(1,1) current regime; a surprising event is y=1 with p<=.25 or y=0 with p>=.75. Two consecutive same-direction surprises with distinct daily evidence families reset active regime to Beta(1+2y,1+2(1-y)). This test does not tune/change the P2 detector.
3. **Rolling14** — Beta(1,1) mean over last 14 completed days, `(1+sum(y[-14:]))/(2+len(y[-14:]))`.
4. **Lagged contextual logistic** — Online logistic regression, separate model per city, initial coefficients all zero; logit = w dot x with features `[1, yesterday_precipitation_positive, yesterday_temp_max/40, yesterday_temp_min/40, yesterday_wind/15, sin(2*pi*day_of_year/365.25), cos(2*pi*day_of_year/365.25)]`. These are available by the forecast time; seasonality is a calendar feature. Predict `sigmoid(clip(logit,-15,15))`. Post-outcome SGD `w_j <- w_j - 0.12 * [(p-y)*x_j + 0.0005*w_j*(j>0)]` (intercept not regularised). No batch fit or ML library required.

All methods use exactly the same future outcomes and chronology. P1/P2 numbers MUST come from the actual compiled C++ MoolBase classes, exported from a native runner; rolling/logistic are evaluated independently in Python stdlib. If native outputs or execution fail, do **not** fill with Python approximations and claim native validation.

## Scoring and gate

Primary endpoint: pooled 2015 mean Brier `mean((p-y)^2)`, P2 vs P1 difference `Delta = Brier(P1)-Brier(P2)` (positive supports P2). **Earn** claim only if pooled Delta >=0.01, block-bootstrap 95% CI lower bound >0, and neither city Delta < -0.02. Otherwise **NOT EARNED**, not a posthoc surprise/benchmark change. Test 2015 both cities, expected 730 daily predictions. Report 2014 separately, not mixed into final test.

Secondary *exploratory* comparisons: rolling14 vs P1, logistic vs P1, each city and pooled; no primary significance claims for these. Report Brier, log loss with p clipped [1e-12, 1-1e-12] for score only, 10-bin expected calibration error (descriptive), event prevalence, counts, total #P2 provisional regime changes per city. No model ranks from hand-picked isolated cases. Report all null and negative outcomes.

Dependence-aware uncertainty: paired circular moving-block bootstrap with **block length 14 days**, resampling blocks of date-index pairs (both cities on the same sampled date), 5000 replicates, random seed `20261018`, percentile 95% CI; this is exploratory given seasonal autocorrelation and only one year. Do not claim causality or robust CI under arbitrary dependence. No posthoc power analysis.

## Negative controls and validity requirements

- P1/P2 native forecast must occur before the current-day outcome is submitted; predictions logged before the evidence label is accessed in the event loop.
- All previous-day meteorological features must be shifted by exactly one day, not same-day values.
- Native C++ receipts use unique `evidence_edge_id` placeholders and per-city/day evidence family IDs for *dataset-record provenance only*. These are not authenticated graph edges or live NOAA verification; name verifier `vega_public_unverified`, not `NOAA_verified`.
- Reject schema drift, missing dates, missing forecasts, malformed CSV, out-of-range predictions, or native/python output mismatch.
- No edits to this protocol after any 2015-score output. Corrections to executable for compilation/validation can be made, but preserve full commit sequence and explicitly disclose them.
- Store raw per-day predictions, source data hash, exact repository SHA, environment, and evaluation JSON as a downloadable CI artifact. Test results are valid only with full files available and matching hash.

## Interpretation bounds

This tests a simple binary frequency model on two geographic weather streams, not multivariable causal models, scientific hypothesis formation, genuine offline incubation, latent neural model training or autonomy. Logistic baseline uses multivariable lagged predictors, but P1 and P2 do not yet learn cross-variable causal relationships. Weather displays seasonality and serial correlation; a two-surprise heuristic may be inappropriate and failure is scientifically useful.

After the test, pursue independent replicated datasets, harder shift diagnostics, truly authenticated evidence, precommitment of forecasts to tamper-evident storage, and learned world-transition operators. Do not merge stacked research PRs by accident.
