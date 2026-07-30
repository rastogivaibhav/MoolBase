# 2WikiMultiHopQA Lyapunov Critic Protocol

## Question

Does the materialised `graphene::LyapunovCritic` assign higher epistemic energy to incomplete, contradictory, duplicated or irrelevant evidence structures than to the gold evidence chain supplied by 2WikiMultiHopQA?

## Scope

This is a component benchmark of the critic and FiberBundle builder. It is not an end-to-end answer-generation benchmark and does not measure answer exact match, retrieval recall or LLM quality.

2WikiMultiHopQA is suitable because every development example contains sentence-level supporting facts and a structured multi-hop evidence path. The dataset does not provide a dependable event-time field, so temporal consistency is held at 1.0. No temporal-performance claim may be made from this run.

## Dataset

- Source content: original 2WikiMultiHopQA, repackaged without content changes by `framolfese/2WikiMultihopQA`.
- Split: validation.
- Sample: 1,000 deterministic examples, seed `20260728`.
- Retrieval: public Hugging Face datasets-server API.
- Recorded fields: question type, number of supporting facts, unique supporting titles, structured evidence count and distractor-title count.

The benchmark stores only compact derived evidence-shape metadata in the workflow workspace and artifact. It does not commit the dataset text to this repository.

## Controlled conditions

For every example the harness constructs these bundles through the real `FiberBundleBuilder` and evaluates them with the real `LyapunovCritic` in empirical mode:

1. `gold`: complete gold supporting-fact chain with full provenance.
2. `missing_hop`: one supporting edge and one source removed, provenance finding added, and retrieval marked truncated.
3. `contradiction`: complete gold path plus a fully sourced contradictory path.
4. `same_source_duplicate`: a second path with a different edge sequence but exactly the same source IDs.
5. `distractor`: complete gold path plus a fully sourced irrelevant path.
6. `wrong_complete`: a structurally complete, fully sourced but deliberately non-gold path.

The final condition is not expected to be separated by a stability-only critic. It tests the explicit claim boundary: Lyapunov energy is not a semantic truth score.

## Primary diagnostics

- missing-hop detection: `V(missing) > V(gold)`;
- contradiction detection: `V(contradiction) > V(gold)`;
- duplicate safety: `V(duplicate) >= V(gold)`;
- distractor safety: `V(distractor) >= V(gold)`;
- repair monotonicity: the controlled `missing -> gold -> gold` trajectory is non-increasing;
- false-stable rate for each corruption;
- same-source independent-path inflation;
- wrong-complete separation rate.

## Initial diagnostic targets

These are engineering targets, not published scientific thresholds:

| Diagnostic | Target |
|---|---:|
| Missing-hop detection | >= 90% |
| Contradiction detection | >= 90% |
| Same-source duplicate not rewarded | >= 98% |
| Irrelevant distractor not rewarded | >= 95% |
| Repair trajectory monotonic | >= 90% |

The first run is diagnostic and always uploads results. A target miss must be reported, not hidden by changing weights on the evaluation sample.

## Leakage control

The fixed validation sample is evaluation data. Critic weights and thresholds must not be tuned on it. Any refinement must use a separately frozen development sample, after which this exact seed and validation sample are rerun unchanged.

## Reproduction

The GitHub workflow `.github/workflows/2wiki-lyapunov.yml` downloads the public metadata, compiles the benchmark directly against the materialised `src/fiber_bundle.cpp` and `src/stability_critic.cpp`, runs the existing direct Lyapunov unit test, and uploads:

- `2wiki_sample.tsv` and manifest;
- per-example `results.csv`;
- `summary.json`;
- `summary.md`.
