# EP-PROCESS-V2 — Cycle 7 Freeze Topology Correction

Status: **PRE-SCORE CORRECTION / NO SCORE RUN OCCURRED**

## What was caught

The first manifest-only authorization PR was merged with GitHub's normal merge method.

The V2 score gate requires the scoring master to be exactly one commit ahead of
the pre-freeze candidate:

```
candidate..HEAD commit count == 1
```

A normal GitHub merge created two commits in that range:

1. the manifest commit;
2. the merge commit.

The content diff was still exactly one file, but the immutable gate would have
rejected the score run because the commit topology did not match the frozen
contract.

## Scientific consequence

**No score-bearing run occurred.**

No candidate outcome was opened.

No candidate oracle join occurred.

Therefore this is a procedural correction before experiment execution, not a
post-result change.

The premature `score_freeze_v2.json` is removed in this correction candidate.

## Corrected freeze procedure

1. Merge this correction and establish a fresh exact candidate master.
2. Run the Cycle-7 pre-freeze gate with the score manifest absent.
3. Record the new candidate commit/tree and pre-freeze evidence.
4. Create a new freeze branch from that exact candidate.
5. Add only `benchmarks/epistemic_process_v2/score_freeze_v2.json`.
6. Merge the manifest PR using **squash**, producing exactly one commit after
   the candidate.
7. Verify:
   - candidate is an ancestor of score master;
   - candidate→master commit count is exactly 1;
   - candidate→master changed paths contain only the score manifest.
8. Only then may `EP-PROCESS-V2-SCORE-001` be dispatched.

## Unchanged scientific assets

This correction does not change:

- the 270-episode task universe;
- task family definitions;
- C0/G0E/C1/G1/G2 profile semantics;
- evaluator logic;
- metric definitions;
- statistical implementation;
- claim thresholds;
- telemetry;
- runtime behavior;
- production reasoning behavior;
- runtime seed;
- bootstrap seed or resample count.

The purpose is solely to make repository topology satisfy the already-frozen
authorization gate before scoring.
