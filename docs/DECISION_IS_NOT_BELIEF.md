# A Decision Is Not a Belief

## GrapheneDB's falsifiable systems thesis

Fast typed decisions are becoming a real machine primitive. That is useful, but it is not the same problem as maintaining an epistemic state through time.

GrapheneDB's public thesis is deliberately narrower than “reasoning database” marketing:

> **GrapheneDB maintains persistent epistemic state; HypoKosh preserves and competes hypotheses; Dialectical Model Worlds (DWM) challenges, reopens and evolves the world model. A decision should not converge merely because one hypothesis has the highest score — it should converge only when the evidence process has earned that convergence.**

This note defines the boundary precisely enough to attack it.

## Three different machine problems

### 1. Typed decision

Input state is mapped to a bounded answer, score or probability.

This is valuable for routing, classification, verification and policy gates. A typed output can remove parsing failures and make control flow cheaper and easier to test.

It does **not by itself** establish:

- whether two supporting observations came from the same source family;
- whether a minority explanation remains viable;
- what evidence caused an earlier belief to be reopened;
- whether a contradiction was resolved or merely outvoted;
- how a later decision relates causally to the evidence state of an earlier one.

### 2. Decision provenance

A system records the state, model, policy, evidence references and outcome associated with a decision. This makes past decisions inspectable and can make replay or audit possible.

That is stronger than a typed decision, but recording why a decision occurred is still different from deciding **when the underlying belief state is allowed to change**.

### 3. Persistent epistemic state

The system carries forward evidence identity, dependency, contradiction, alternative hypotheses and revision history. New evidence can preserve, challenge, reopen or replace a prior world state under explicit rules.

This is the layer GrapheneDB is attempting to test.

## The GrapheneDB stack

```text
persistent evidence + provenance
            |
        GrapheneDB
            |
 competing hypotheses
            |
         HypoKosh
            |
 challenge / reopen / synthesis
            |
            DWM
            |
 governed belief state + native epistemic receipt
```

The distinction is architectural, not semantic branding:

- **GrapheneDB** owns durable evidence identity, lineage and epistemic state.
- **HypoKosh** owns competing-hypothesis runtime behavior.
- **DWM** owns challenge, reopen and synthesis behavior.
- **Native epistemic receipts** expose which layers actually executed and why state changed.

## The claim must survive ablation

If the layers above matter, they must produce measurable effects that cannot be explained by relabelling the same execution.

The preregistered architecture comparison therefore requires real execution boundaries:

```text
B0  bounded/stateless baseline
G0  GrapheneDB persistence/provenance only
G1  G0 + HypoKosh competing hypotheses
G2  G1 + DWM challenge/reopen/synthesis
```

A valid G0 run cannot execute HypoKosh and merely hide its events. A valid G1 run cannot execute DWM and merely suppress its receipts. Disabled layers must be mechanically absent from execution.

This is why the current G0 implementation gap is treated as a scientific blocker rather than patched in the benchmark adapter.

## What would falsify the thesis?

The GrapheneDB thesis should be weakened if controlled, frozen evaluation shows any of the following:

1. **Persistence adds no useful signal.** G0 does not improve recovery, provenance fidelity or resistance to duplicated/correlated evidence over B0.
2. **Hypothesis competition adds no useful signal.** G1 does not preserve viable minority explanations or reduce false convergence relative to G0.
3. **Dialectical reopening adds no useful signal.** G2 does not improve recovery after contradiction/late counterevidence, or it creates enough unnecessary reopening to erase the benefit.
4. **Receipts do not explain revision.** An independent evaluator cannot reconstruct which evidence and challenge caused a belief transition.
5. **The effect disappears outside curated fixtures.** Independent users cannot reproduce the mechanism on clean checkout or on their own admissible scenarios.

Null and negative layer results are publishable results. They are implementation priorities, not reasons to alter a frozen scored protocol.

## What GrapheneDB does not claim

GrapheneDB does not currently claim:

- general semantic truth;
- superiority to all agent-memory systems;
- superiority to typed decision models on classification, latency or cost;
- that structural stability implies factual correctness;
- that an internal benchmark constitutes independent validation;
- enterprise GA readiness.

A fast calibrated decision primitive can be complementary to GrapheneDB. For example, a typed model may score or classify an observation while GrapheneDB/HypoKosh/DWM governs how that observation changes a persistent belief state.

## Why this matters now

The surrounding ecosystem is converging on several adjacent but distinct layers: fast typed decisions, persistent agent memory, decision provenance, replay/audit infrastructure and repository-native decision records. That is good pressure on GrapheneDB: provenance alone is not enough differentiation, and “agent memory” is too broad a category claim.

The defensible question is narrower:

> **Can a machine preserve competing explanations and make belief revision causally inspectable under changing, duplicated and contradictory evidence — while refusing convergence when the evidence process has not earned it?**

The Lab & Market Program exists to answer that question empirically.

## Reproduce or attack it

Start with:

- `docs/INDEPENDENT_REPRODUCTION.md`
- `docs/REASONING_MODES_AND_RECEIPTS.md`
- GitHub issue #25 — Lab & Market Program
- GitHub issue #40 — Epistemic Process Evaluation v1
- GitHub issue #53 — honest G0/G1/G2 production runtime boundaries

The most useful external contribution is a reproduction, counterexample, adversarial scenario, ablation critique or evidence that one of the proposed layers is unnecessary.
