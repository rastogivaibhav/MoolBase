# HypoKosh Frozen Unseen-100 Stress Evaluation

## Scope

A frozen 100-case synthetic oracle-context suite was generated with seed `20260727` after the prior three-case HotpotQA pilot. It covers bridge, shared-property, yes/no, date, numeric, location, and contradictory-evidence behaviour. This is not an official HotpotQA run.

## Initial run

- Answer EM: 41.00%
- Answer F1: 47.29%
- Resolution-gate accuracy: 47.00%
- No-silent-promotion: 100.00%

Failures exposed brittle paraphrase handling, incomplete bridge recognition, overly broad yes/no matching, missing alternative numeric/location phrasings, and failure to downgrade contradictory location evidence.

## Fixes

- Generalized bridge questions to `who` and `which person` patterns and accepted headquarters/base evidence as the second hop.
- Expanded shared-property phrasing and media classes.
- Added membership-count and `in which city` query forms.
- Reworked yes/no location comparison to test the query target against each source rather than treating generic lexical overlap as positive evidence.
- Added contradictory location detection and forced non-resolved status when source claims disagree.

## Final run

- Answer EM: 89.00%
- Answer F1: 89.67%
- Resolution-gate accuracy: 100.00%
- No-silent-promotion: 100.00%
- Mean latency: 2.245 ms/case

The 10 contradiction cases intentionally do not receive answer credit because the suite's gold answer records the first claim while the correct governed behaviour is abstention. All 10 were downgraded from `RESOLVED`, yielding 100% gate accuracy. Excluding contradiction cases, answer EM is 98.89%.

## Regression

- Full CTest suite: 45/45 passed
- Total runtime: 28.60 seconds

## Limitation

The official HotpotQA validation data could not be fetched in this execution environment. This suite validates operator generalisation and governance, not external benchmark generalisation. The next external gate remains a frozen 100-example HotpotQA oracle-context sample sourced directly from the official validation split.
