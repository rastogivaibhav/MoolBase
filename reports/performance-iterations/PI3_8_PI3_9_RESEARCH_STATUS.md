# PI3.8 / PI3.9 canonical research status

Date: 2026-08-19

This file records evidence from the canonical local GrapheneDB research line. It does **not** mean the corresponding source tree has been materialized into this GitHub branch.

## Remote materialization boundary

PR #20 remains a draft reconciliation PR. The accepted canonical source line is still ahead of remote `master`, and the complete source tree has not been atomically synchronized through the available connector. Do not merge this PR as the complete implementation.

## PI3.8 target closeout

The previously blocked real-model target was executed with `all-MiniLM-L6-v2` at 384 dimensions over the full LoCoMo workload (5,882 memory nodes; 1,977 scoreable questions).

PI3.7 lexical-semantic baseline:
- Hit@1: 0.036418816388467376
- Hit@5: 0.06980273141122914
- Hit@10: 0.07991906929691452
- MRR@10: 0.05121335838331286

MiniLM 384D:
- Hit@1: 0.023773394031360646
- Hit@5: 0.05513404147698533
- Hit@10: 0.07486090035407182
- MRR@10: 0.03667172483560953

Decision: **PI3.8 target-quality FAIL**. The target is no longer blocked; MiniLM alone does not justify neural semantic promotion.

## PI3.9 hybrid retrieval research

A fixed reciprocal-rank fusion policy (`RRF`, `k=60`) was evaluated using the PI3.7 lexical-semantic ranking plus MiniLM ranking. QA answers/evidence labels were not used to fit the representations or fusion rule.

Hybrid RRF60 full-corpus result:
- Hit@1: 0.03945371775417299
- Hit@5: 0.07587253414264036
- Hit@10: 0.09155285786545271
- MRR@10: 0.0547428362678742

The hybrid beats the PI3.7 baseline on all four point metrics. A 3,000-sample paired query bootstrap gives a positive 95% lower bound for the Hit@10 improvement; Hit@1 and MRR confidence intervals still cross zero.

Candidate depth 100:
- lexical evidence Hit@100: 0.12392513909964593
- neural evidence Hit@100: 0.1340414769853313
- union evidence Hit@100: 0.15933232169954475
- mean top-100 Jaccard: 0.09555775035848868

Decision: **PI3.9 accepted as a controlled research/benchmark increment only. Production hybrid retrieval is NOT approved.**

## Fresh local acceptance rerun

The sealed canonical PI3.9 research snapshot was revalidated locally:
- permanent PI3.9 gate: PASS
- full Release product regression: 68/68 PASS, 0 failures, 13.28s CTest real time
- CSuite: 15/15 correct causal roots
- authoritative OFF/ON shadow mismatches: 0
- shadow top-1 agreement: 100%
- shadow mean/worst Recall@k: 1.000 / 1.000
- governed reasoning remained `evidence_required`
- no PI3.9 production changes under `src/` or `include/`

A stricter conversation-clustered benchmark was attempted but did not produce a completed canonical result before evidence sealing, so no partial result is counted.

## Sealed local artifacts

Local PI3.9 commit: `3e18619cdfbb874675d4b9d0475ac212593855db`

- source ZIP SHA-256: `af3a99823085d1a53ceb7bfd997590bb29ac3e1b0c8fc1daa306a6d498d1f210`
- evidence ZIP SHA-256: `94dd075a6817dabacedbd65fe16ab2038afca935701c3495d79dc8b855a6314f`
- PI3.8 -> PI3.9 patch SHA-256: `8369f6eeb69b089f5be1e43b90e007ef1e7ec3948a76ea92e309c2870733633f`

These hashes identify the validated local artifacts; they are not GitHub tree hashes.

## Authority boundary

Nothing in PI3.8/PI3.9 approves:
- `VectorIndexKind::Auto` changes;
- CLI/server default changes;
- a durable neural/shadow format;
- replacing Exact Flat authority;
- causal/Hex authority changes;
- HypoKosh/FiberBundle governance changes;
- answer authority for semantic candidate sources.
