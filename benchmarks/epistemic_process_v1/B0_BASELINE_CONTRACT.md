# Epistemic Process v1 — B0 baseline contract

Status: **pre-freeze candidate**. This document describes the already implemented
B0 behaviour; it does not change episode semantics or expected answers.

## Purpose

B0 is the deliberately small architecture control for the B0/G0/G1/G2
ablation. It answers one question only:

> What happens when each observation is handled locally without persistent
> epistemic state, Graphene evidence retrieval, competing-hypothesis reasoning,
> or dialectical challenge/reopen?

B0 is not intended to represent the strongest available external agent. Beating
B0 alone is therefore not sufficient evidence for the overall MoolBase thesis.

## State model

- retained across steps: **none**
- discarded after each step: **all prior observations**
- GrapheneDB: **not used**
- Hypothesis Engine: **not used**
- DWM: **not used**
- persistent provenance/history: **not used**

## Observation visibility

At step N, B0 receives only the current preregistered observation fields needed
for its bounded decision:

- event id;
- event kind;
- hypothesis the observation bears on.

It does not receive terminal_supported, decisive, the expected answer,
benchmark correctness, future evidence, or evaluator-only independence labels.

## Deterministic decision rule

For a current observation with kind == support:

- emit status = provisional;
- emit the observation's bears_on hypothesis;
- cite only that current evidence id.

For a non-support observation:

- emit status = abstain;
- emit no hypothesis;
- cite only that current evidence id.

B0 never reconstructs history or converts previous observations into state.

## Contradiction behaviour

Contradiction/refutation is not retained. A current non-support observation
causes local abstention; it does not reopen or revise a stored hypothesis because
B0 has no persistent hypothesis state.

## Provenance and history

B0 may echo the current evidence identity into its receipt so the evaluator can
audit what observation caused the local decision. It does not retain provenance
relationships or provide historical replay.

## Scientific interpretation

B0 is acceptable only as a simple deterministic lower-bound architecture
control. The primary causal comparisons for architecture value remain:

- G0 versus B0: persistent evidence/provenance contribution;
- G1 versus G0: competing-hypothesis contribution;
- G2 versus G1: dialectical challenge/reopen contribution.

Once the score freeze is created, B0 is immutable for Epistemic Process v1.
