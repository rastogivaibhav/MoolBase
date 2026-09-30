# EP-PROCESS-V2 — Cycle 1 G2 Revision Diagnosis

Status: diagnostic evidence only; no production change; V1 remains immutable.

## Provenance

- Frozen experiment: `EP-PROCESS-V1-SCORE-001`
- Frozen scoring commit: `3c008577739b83a94defc47c73275bf8ba76e76c`
- Workflow artifact id: `11079766673`
- Artifact ZIP SHA-256: `3787cdb173a816a034229bd3faa9f086c9df5f47375feb99cf44a61a9e3e4fe9`
- Primary preserved inputs: `raw-G2.json`, `receipts-G2.json`, `scores-G2.json`, `aggregate-report.json`.
- Source path used to explain revision semantics: `src/hypokosh_runtime_frontier_part_3.inc` at frozen master.

## Executive finding

G2 emitted **19 challenge events and 19 reopen transitions, but zero revision events**. The preserved event stream proves that for all 19 reopens the hypothesis selected by the initial convergence of that `reason()` call was the same hypothesis present at the terminal event after re-expansion. Therefore the frozen production revision predicate was false in every reopen: the call began with an answer, ended with an answer, and the primary node did not change.

This does **not** prove that re-expansion discovered nothing. The V1 runner did not preserve the recovery trace, initial/final bundle hashes, per-target support/opposition strengths, or initial/final candidate ranking. Those questions remain observability gaps and must not be guessed.

A second finding is more important for V2 measurement design: the runtime's `Revision` event compares initial vs reopened convergence **inside one `reason()` call**. It does not encode a change from the previous observation step. In EP01, EP03 and EP04, the new decisive evidence caused the next call's initial convergence to move from H1 to H2 before DWM reopened anything. Those prompt cross-step changes therefore received no `revision` event and no V1 refutation-response credit.

## Event accounting

| Episode | G2 challenges | G2 reopens | G2 revisions | Decisive event | Prior-step hypothesis | Hypothesis at decisive step | V1 refutation response |
|---|---:|---:|---:|---|---|---|---|
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 5 | 5 | 0 | refute @ step 4 | H1 | H2 (open) | no |
| `EP02_LATE_REVOCATION` | 4 | 4 | 0 | revoke @ step 4 | H1 | H1 (open) | yes |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 6 | 6 | 0 | refute @ step 5 | H1 | H2 (open) | no |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 4 | 4 | 0 | refute @ step 3 | H1 | H2 (open) | no |

Total: **19 challenges / 19 reopens / 0 revisions**.

## Every reopen checked against the production revision predicate

Frozen predicate:

```cpp
const bool revised_answer =
    previous_has_answer != reopened_convergence.has_answer ||
    (previous_has_answer && reopened_convergence.has_answer &&
     previous_primary_node != reopened_convergence.primary_node);
```

The frozen receipts do not expose `has_answer` directly, but each challenged call contains an initial `decision` with state `selected`, and each terminal carries the same selected hypothesis node. No challenged call emitted a `revision`.

| Episode | Step | Initial | Reopen from | Terminal | Terminal state | Classification |
|---|---:|---|---|---|---|---|
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 1 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 2 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 3 | H1 | H1 | H1 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 4 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP01_DUPLICATE_SUPPORT_THEN_REFUTE` | 5 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP02_LATE_REVOCATION` | 1 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP02_LATE_REVOCATION` | 3 | H1 | H1 | H1 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP02_LATE_REVOCATION` | 4 | H1 | H1 | H1 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP02_LATE_REVOCATION` | 5 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 1 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 2 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 3 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 4 | H1 | H1 | H1 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 5 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP03_CORRELATED_MAJORITY_MINORITY_WINS` | 6 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 1 | H1 | H1 | H1 | provisionally_resolved | `ANSWER_STATE_UNCHANGED` |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 2 | H1 | H1 | H1 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 3 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |
| `EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE` | 4 | H2 | H2 | H2 | contested | `ANSWER_STATE_UNCHANGED` |

All 19 observable initial→terminal hypothesis pairs are unchanged. This explains why the frozen revision event was not emitted, but cannot establish whether confidence, ranking margins, or the underlying bundle changed while preserving the same primary hypothesis.

## Decisive-event diagnosis

### EP01_DUPLICATE_SUPPORT_THEN_REFUTE

- Prior step: H1.
- At decisive step 4 the current call had already selected H2 before the DWM reopen.
- DWM reopened H2→H2, so no within-call revision event could fire.
- V1 refutation-response result: not responded.

### EP02_LATE_REVOCATION

- Prior step: H1.
- At decisive step 4 the current call still selected H1.
- DWM reopened H1→H1. V1 counted this as a refutation response because the reopen originated from contradicted H1.
- The actual H1→H2 selection occurred at step 5 and was not encoded as a revision event.

### EP03_CORRELATED_MAJORITY_MINORITY_WINS

- Prior step: H1.
- At decisive step 5 the current call had already selected H2 before the DWM reopen.
- DWM reopened H2→H2.
- V1 refutation-response result: not responded.

### EP04_REFUTE_WITHOUT_ENOUGH_REPLACEMENT_EVIDENCE

- Prior step: H1.
- At decisive step 3 the current call had already selected H2 before the DWM reopen.
- DWM reopened H2→H2.
- The task expected ABSTAIN, so this prompt H1→H2 change is not a correctness success; the system still over-committed to replacement evidence.
- V1 refutation-response result: not responded.

## What is proved

- DWM genuinely executes native challenge/reopen behavior.
- Every G2 challenge in this V1 run caused one reopen.
- None of the 19 within-call reopens changed the operative hypothesis.
- EP01, EP03 and EP04 changed operative hypothesis across observation calls before DWM's reopen loop.
- EP02's decisive revoke produced H1→H1 at reopen, then H2 on the next observation call.
- The V1 refutation-response metric is not a clean measure of actual cross-step belief change for this workload.

## What is not proved

- Byte-identical bundles after re-expansion.
- Unchanged confidence/ranking margins.
- A DWM production defect solely from zero revisions.
- Any G2 terminal-accuracy gain over G1.
- Absence of epistemic value: G2 visibly changes terminal status to contested/open in cases where G1 stays provisional.

## Observability gaps to preregister for V2

V2 should freeze, per reason-call and recovery round:

1. prior observation-step operative hypothesis and status;
2. initial and reopened `has_answer` plus primary node;
3. initial/reopened bundle immutable hash;
4. visited states and truncation before/after reopen;
5. search-option deltas and explicit reopen nodes;
6. per-target support, opposition, belief strength, verification status and independent-family count;
7. target rank ordering before/after reopen;
8. an explicit cross-step `belief_change` event distinct from within-call recovery `revision`.

## Cycle 1 conclusion

The zero-revision result is classified as **expected under the frozen event semantics plus an observability/temporal-measurement gap**, not as a proven production defect. V2 must make cross-step belief change observable and then test whether DWM re-expansion causally changes frontier, ranking, status, or final answer.
