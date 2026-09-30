# Cycle 03 — G2 / DWM Forensic Diagnosis

## Question

Why did V1 produce 19 Challenge events and 19 Reopen transitions but zero Revision transitions?

## Native path

When DWM is enabled and opposition requests re-expansion, the production runtime performs:

```text
opposition -> Challenge -> reopen options -> Reopen
          -> re-expand the same resolved database snapshot
          -> reassess stability/admissibility
          -> reconverge
          -> emit Revision only if has_answer or primary hypothesis changed
```

The implementation compares the pre-reopen convergence and post-reopen convergence **inside the same runtime call**.

## Frozen V1 evidence

Across the four G2 episodes, the artifact contains 19 Challenge events and 19 Reopen transitions.

Every reopen transition is same-hypothesis at the event level (for example H1 -> H1 or H2 -> H2). No native Revision event follows.

At the same time, the sequence of benchmark decisions does change H1 -> H2 after later evidence arrives in all four episodes. Those changes occur between calls, not within the DWM reopen loop.

The V1 runtime summaries show repeated bounded recovery/re-expansion, commonly one expansion round, while terminal causes frequently remain `dialectic_opposition_blocks_resolution` or `material_contradiction_blocks_resolution`.

## Causal conclusion

The V1 DWM loop was active, but it was usually re-searching a snapshot whose newly arrived evidence had already been considered by the initial convergence of that call.

A Challenge/Reopen therefore frequently changed the **process trace** without changing the **operative answer**.

The absence of native Revision is consistent with the implementation: Revision is only emitted when re-expansion itself changes the answer.

## What V1 supports

- DWM is not inert; it genuinely triggers opposition, challenge and reopen machinery.
- DWM can alter process-level behaviour.
- In V1 it did not improve terminal accuracy over G1.
- V1 observed only one of four eligible episodes satisfying the frozen refutation-response criterion.

## What remains unknown

V1 receipts did not expose enough per-reopen detail to distinguish every Challenge/Reopen as:

- useful frontier expansion;
- changed bundle but unchanged answer;
- unchanged bundle/no progress; or
- repeated opposition churn.

## V2 instrumentation requirement

For every DWM reopen round, preserve at least:

- previous and next bundle hash;
- previous and next visited-state count;
- frontier-progress flag;
- bundle-changed flag;
- previous/current primary hypothesis;
- answer-changed flag;
- stop reason;
- expansion round count.

V2 must score **reopen effectiveness** separately from raw Challenge/Reopen count.

## Status

**Cycle 03 exit gate: PASS.** The causal path and V1 measurement limitation are identified.
