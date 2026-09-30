# Cycle 02 — G1 / HypoKosh Forensic Diagnosis

## Question

What did HypoKosh add in V1, and could V1 validly test temporal belief revision?

## Source trace

With HypoKosh enabled, `CompleteHypoKoshRuntime::reason` constructs the dialectic expansion, FiberBundle, stability assessment, epistemic admissibility, governed convergence and a native HypoKosh `Decision` event.

G1 therefore adds a genuine hypothesis-selection/governance layer that G0 intentionally lacks.

The runtime can also emit a native `Revision` event, but only inside one `reason()` call when bounded re-expansion changes either:

- whether an answer exists; or
- the selected primary hypothesis.

## Temporal measurement mismatch in V1

The V1 benchmark ingests one new evidence item and then calls `runtime.reason(...)` from scratch for that new database snapshot.

The previous step's operative hypothesis is not supplied to the next call as the prior decision state. `update_model_world` is also disabled in the V1 runner.

Therefore a change such as:

```text
step 3: H1
step 4 after new evidence: H2
```

is represented as two fresh native Decision/Terminal sequences, not as a native Revision from H1 to H2.

The V1 adapter deliberately refuses to invent revision events from changed outputs. It only creates `revise` transitions from native `Revision` events.

The V1 evaluator's refutation-response metric likewise requires a `revise` or `reopen` transition sourced from the contradicted hypothesis.

## Evidence from the frozen V1 artifact

The frozen G2 receipts visibly change operative hypotheses across benchmark steps in multiple episodes:

- EP01: H1 -> H2 between steps 3 and 4.
- EP02: H1 -> H2 between steps 4 and 5.
- EP03: H1 -> H2 between steps 4 and 5.
- EP04: H1 -> H2 between steps 2 and 3.

Yet there are zero native `Revision` events, because the change occurs between separate calls after new evidence is ingested.

## Causal conclusion

V1 validly tested native **intra-call** revision during bounded recovery/re-expansion.

V1 did **not** have a valid native measurement boundary for **inter-step temporal belief revision caused by newly arriving evidence**.

Thus “zero revisions” is a correct V1 observation but must not be generalized to “HypoKosh never changes its mind.”

## What G1 V1 supports

- HypoKosh adds governed hypothesis selection beyond G0.
- G1 restores answer coverage on the four V1 tasks.
- G1 did not emit native intra-call Revision events in V1.

## What remains unproved

- Whether temporal H1 -> H2 changes after new evidence are reliably correct.
- Whether those changes are explicitly auditable as revisions.
- Whether HypoKosh improves decision quality over a fair decision-capable baseline at larger scale.

## V2 requirement

Measure two separate properties:

1. **temporal hypothesis change**: consecutive production decisions differ after new evidence;
2. **native revision receipt**: the system explicitly records causal revision provenance.

Do not treat one as a substitute for the other.

## Status

**Cycle 02 exit gate: PASS.**
