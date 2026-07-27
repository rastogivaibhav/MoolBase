# HypoKosh Official HotpotQA Sample-3 Fix and Retest

## Scope

This iteration fixes the answer-contract failures exposed by the frozen compact official HotpotQA development sample. The three cases were not changed after the failed run.

## Fixes

- Added typed birth-date comparison for “Who is older?” questions.
- Added controlled equivalence between `American` and `from the United States` for nationality comparisons.
- Added entity-slot bridge reasoning for portrayal-to-person-to-government-position questions.
- Added answer-contract enforcement so a rendered evidence path cannot qualify as a resolved natural-language answer.
- Preserved evidence receipts and no-silent-promotion behaviour.

## Results

| Case | Gold | Before | After | Status |
|---|---|---|---|---|
| Age comparison | Terry Richardson | reasoning path | Terry Richardson | RESOLVED |
| Nationality yes/no | yes | reasoning path | yes | RESOLVED |
| Entity-role bridge | Chief of Protocol | no answer / HITL | Chief of Protocol | RESOLVED |

- Answer exact match: **3/3 (100%)**
- Correct resolved answers: **3/3 (100%)**
- No silent promotion: **3/3 (100%)**
- Evidence: two source atoms retained per answer

## Regression

- `graphenedb_recursive_model_world_tests`: passed
- Complete CTest suite: **45/45 passed**

## Interpretation

This closes the specific defects found by the compact official sample. It does not establish broad HotpotQA generalisation; the next gate is the remaining eight questions in the same public compact sample, followed by a larger frozen external sample.
