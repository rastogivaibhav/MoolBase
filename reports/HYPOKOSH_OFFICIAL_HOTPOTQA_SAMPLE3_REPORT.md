# HypoKosh Official HotpotQA Sample-3 Test

## Scope

Three official HotpotQA development examples were sourced from the public MultiQA sample artifact at commit `7115eea27d3c0473c0c709b03b5c33884c912fd8`. Only the annotated supporting context was supplied, so this is an oracle-context reasoning and synthesis test rather than a retrieval test.

## Results

| ID | Gold | Status | Exact match | Observation |
|---|---|---|---:|---|
| 5a7bbb64554299042af8f7cc | Terry Richardson | RESOLVED | 0 | Returned an evidence chain instead of comparing birth dates. |
| 5a8db19d5542994ba4e3dd00 | yes | RESOLVED | 0 | Returned both evidence sentences instead of a yes/no answer. |
| 5a8c7595554299585d9e36b6 | Chief of Protocol | HITL_REQUIRED | 0 | Failed the bridge from character to actor to government position. |

Answer EM was **0/3**. No-silent-promotion remained true in all three executions, but two outputs violated the intended `RESOLVED` contract because they were reasoning traces rather than concise query-compatible answers.

## Interpretation

The earlier synthetic operator suite overestimated generalization. The immediate defect is not GrapheneDB storage or graph traversal. It is the HypoKosh question-conditioned synthesis and resolution gate: unsupported question patterns fall back to a path rendering, and the resolver can still label that rendering `RESOLVED`.

## Required fix before larger benchmark runs

1. Enforce an answer-shape validator for every question class.
2. Never mark a path rendering as `RESOLVED`.
3. Add comparative date/age reasoning.
4. Add nationality/origin normalization for yes/no comparison.
5. Add open-domain bridge extraction that follows entity mentions instead of fixed templates.
6. Re-run these frozen three cases, then expand to the remaining eight examples in the same official sample artifact.
