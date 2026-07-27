# HypoKosh Answerable Reasoning Iteration

## Scope

This iteration adds a governed, query-conditioned answer synthesis stage after graph reasoning. It separates the concise answer from the retained reasoning trace and minimises the supporting evidence presented for the answer.

Implemented operators:

- shared-property/media-type answer extraction;
- bridge/entity answer extraction;
- categorical yes/no synthesis;
- minimum sufficient evidence selection;
- user-facing `RESOLVED` recalibration.

The synthesiser consumes observed source atoms. It does not promote inferred or hypothetical relations to observed truth.

## Frozen three-case HotpotQA oracle-context result

| Metric | Before | After |
|---|---:|---:|
| Answer exact match | 0.00% | 100.00% |
| Answer F1 | 2.89% | 100.00% |
| Supporting-fact exact match | 33.33% | 66.67% |
| Supporting-fact F1 | 61.90% | 83.33% |
| Joint exact match | 0.00% | 66.67% |
| Joint F1 | 0.81% | 83.33% |
| No silent truth promotion | 100.00% | 100.00% |

Answers produced:

1. `video game`
2. `Robert Digges Wimberly Connor`
3. `yes`

All three cases were marked `RESOLVED` only after a concise, query-compatible answer and at least two supporting evidence atoms were available.

## Validation

- `graphenedb_cli` built successfully.
- `graphenedb_recursive_model_world_tests` passed.
- Frozen deterministic and model-backed pilots both produced 100% answer EM/F1.
- The all-target build was started but exceeded the execution time budget while compiling unrelated large stress targets; no compiler failure occurred in the changed reasoning target.

## Remaining limitation

The bridge case selects the correct identity sentence and a semantically relevant NARA sentence, but one selected NARA ordinal differs from the HotpotQA gold supporting-fact annotation. This is why supporting-fact and joint exact match are 66.67%, not 100%.
