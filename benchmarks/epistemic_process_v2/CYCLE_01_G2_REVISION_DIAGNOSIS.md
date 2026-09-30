# EP-PROCESS-V2 — Cycle 1 G2 Revision Diagnosis

Status: diagnostic evidence only; no production change; V1 remains immutable.

## Provenance

- Frozen experiment: `EP-PROCESS-V1-SCORE-001`
- Frozen scoring commit: `3c008577739b83a94defc47c73275bf8ba76e76c`
- Workflow artifact id: `11079766673`
- Artifact ZIP SHA-256: `3787cdb173a816a034229bd3faa9f086c9df5f47375feb99cf44a61a9e3e4fe9`

## Executive finding

G2 emitted **19 challenge events and 19 reopen transitions, but zero revision events**. For all 19 reopens, the hypothesis selected at the start of that `reason()` call was the same hypothesis present at the terminal event after re-expansion. The frozen revision predicate therefore did not fire.

This does not prove that the internal bundle, confidence, or ranking margin stayed identical. V1 did not preserve the recovery trace, before/after bundle hashes, or per-target scores. Those are observability gaps.

More importantly, V1's `Revision` event only captures a change **inside one `reason()` call**. It does not encode an H1→H2 change from the previous observation step. EP01, EP03 and EP04 changed from H1 on the prior step to H2 at the decisive step before DWM performed its reopen, so those prompt cross-step changes received no revision credit.

## Event accounting

| Episode | Challenges | Reopens | Revisions | Decisive event | Prior-step hypothesis | Decisive-step hypothesis | V1 response |
|---|---:|---:|---:|---|---|---|---|
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 5 | 5 | 0 | refute @ 4 | H1 | H2 (open) | no |
| `EP02_LATE_REVOCATION` | 4 | 4 | 0 | revoke @ 4 | H1 | H1 (open) | yes |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 6 | 6 | 0 | refute @ 5 | H1 | H2 (open) | no |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 4 | 4 | 0 | refute @ 3 | H1 | H2 (open) | no |

Total: **19 challenges / 19 reopens / 0 revisions**.

## Frozen revision predicate

```cpp
const bool revised_answer =
    previous_has_answer != reopened_convergence.has_answer ||
    (previous_has_answer && reopened_convergence.has_answer &&
     previous_primary_node != reopened_convergence.primary_node);
```

Every challenged call preserved by V1 had the same observable initial and terminal hypothesis:

- EP01: H1→H1 at steps 1–3; H2→H2 at steps 4–5.
- EP02: H1→H1 at steps 1, 3, 4; H2→H2 at step 5.
- EP03: H1→H1 at steps 1–4; H2→H2 at steps 5–6.
- EP04: H1→H1 at steps 1–2; H2→H2 at steps 3–4.

All 19 no-revision cases are therefore classified at the observable answer layer as `ANSWER_STATE_UNCHANGED`.

## Decisive-event diagnosis

### EP01

The previous step operated on H1. At the decisive refutation step, initial convergence already selected H2. DWM then reopened H2→H2. The system changed across observation calls, not inside the reopen loop, so V1 gave no refutation-response credit.

### EP02

The previous step and decisive revoke step both selected H1. DWM reopened H1→H1. V1 counted this as a response because the reopen originated from contradicted H1, even though the operative hypothesis did not change. H2 was selected on the following observation step.

### EP03

The previous step operated on H1. At the decisive refutation step, initial convergence already selected H2. DWM reopened H2→H2. The cross-step change received no revision credit.

### EP04

The previous step operated on H1. At decisive refutation, initial convergence selected H2 and DWM reopened H2→H2. The task expected ABSTAIN, so this is not a correctness success; it shows that the cross-step event was invisible while the system still over-committed to replacement evidence.

## What is proved

- DWM genuinely emitted native challenges and reopens.
- Every V1 G2 challenge caused one reopen.
- No within-call reopen changed the operative hypothesis.
- Three decisive evidence arrivals changed the operative hypothesis across observation calls before DWM reopening.
- EP02 received response credit for reopen-without-hypothesis-change.
- The V1 refutation-response metric is not a clean measure of actual cross-step belief change on this workload.

## What remains unknown

V1 cannot prove whether any reopen changed:
- bundle hash/content;
- target score margins;
- target rank ordering beneath the unchanged winner;
- before/after visited-state frontier;
- search-option effectiveness.

## V2 telemetry required

Freeze these before V2 scoring:
1. previous observation-step hypothesis/status;
2. initial and reopened `has_answer` and primary node;
3. initial/reopened bundle hash;
4. visited states and truncation before/after reopen;
5. search-option deltas;
6. per-target support/opposition/belief strength;
7. rank order before/after reopen;
8. an explicit cross-step `belief_change` event distinct from within-call `revision`.

## Conclusion

Zero V1 revisions are **explained by unchanged within-call answer state plus a temporal/observability gap**. They are not, by themselves, proof of a DWM production defect.
