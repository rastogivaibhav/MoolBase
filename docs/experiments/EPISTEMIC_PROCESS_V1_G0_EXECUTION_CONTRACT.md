# Epistemic Process v1 — G0 production execution contract

Status: **pre-implementation contract; UNSCORED only**

Governing programme: #25  
Experiment: #40  
Runtime capability gap: #53  
Production-runner gate: #51

## Purpose

G0 is the GrapheneDB-only arm of the preregistered B0/G0/G1/G2 architecture ablation. It exists to answer one narrow question:

> What does persistent GrapheneDB evidence/provenance state contribute before HypoKosh competing-hypothesis reasoning and the DWM challenge/reopen/synthesis loop are enabled?

G0 must be a real production execution boundary. It must never be implemented by running HypoKosh/DWM and suppressing their events afterward.

This document fixes the implementation acceptance boundary before G0 exists. It does **not** change tasks, event order, evidence-family/dependency semantics, evaluator metrics, terminal expectations, or any frozen/score-bearing benchmark artifact.

## Required capability profile

For G0:

```text
GrapheneDB persistent evidence/provenance: ON
HypoKosh competing-hypothesis runtime:       OFF
DWM challenge/reopen/synthesis:              OFF
Opposition-led research:                     OFF
Model-world hypothesis promotion:            OFF by default for the ablation
```

`enable_hypokosh=false` with `enable_dwm=true` remains invalid and must fail explicitly.

## Production ownership

The G0 path must live in production C++ runtime/library code. The benchmark adapter may select the profile and translate native receipts; it may not decide what GrapheneDB should believe.

The production path may use GrapheneDB-owned evidence/provenance retrieval and immutable bundle construction required to expose stored evidence state. It must not instantiate or execute the HypoKosh competition/controller path or any DWM opposition/challenge/reopen/synthesis path.

If an existing class has mixed Graphene/HypoKosh responsibilities, extract the Graphene-owned evidence/provenance operation into a production abstraction rather than duplicating benchmark logic.

## Receipt evidence

A successful G0 call must mechanically report:

```text
hypokosh_capability_enabled = false
dwm_capability_enabled      = false
graphene_executed           = true
opposition_executed         = false
```

It must emit **no** native event whose source is `HypoKosh` or `DialecticalModelWorlds`.

Absence of events alone is insufficient. The receipt must distinguish a disabled layer from a layer that executed but happened to emit nothing.

Graphene-owned terminal/evidence-state events are permitted only when they describe state actually produced by the Graphene-only path; they must not synthesize HypoKosh decisions, challenges, reopens or revisions.

## Anti-oracle boundary

The G0 production path must not consume or reproduce benchmark scoring knowledge, including:

- `terminal_supported`;
- `decisive`;
- expected terminal answer/state;
- evaluator-only independence judgments;
- `run_v1.py` helpers such as `_choose`, `_is_refuted`, or `_independent_support`.

The system under test receives observations/evidence and runtime configuration only.

## Minimum tests before #51 may use G0

### 1. Capability dependency

Requesting DWM with HypoKosh disabled fails explicitly.

### 2. No hidden HypoKosh execution

A G0 fixture that would normally produce multiple competing hypotheses completes with `hypokosh_capability_enabled=false`, and the native receipt contains zero HypoKosh events.

### 3. No hidden DWM execution

A G0 fixture containing material opposition completes with `dwm_capability_enabled=false`, `opposition_executed=false`, and zero DWM challenge/reopen/revision events.

### 4. Graphene actually executed

The same calls report `graphene_executed=true` and retain native evidence/provenance identifiers sufficient for the adapter to inspect what Graphene stored/retrieved.

### 5. Default behaviour remains compatible

Calls that do not explicitly select an ablation profile preserve the existing production behaviour and current tests.

### 6. Leakage guard

The pre-freeze integrity test continues to reject benchmark scoring metadata/oracle helpers in the production-runner decision path.

## Clean-checkout gate

After implementation, #51 may advance only when a clean checkout can run every preregistered episode in B0/G0/G1/G2 **UNSCORED** mode and preserve:

- raw native event streams;
- native receipts;
- adapter/runtime/schema failures;
- configuration capability evidence;
- commit/compiler/OS identity;
- protocol/task/evaluator/adapter/runner hashes;
- determinism/seed statement.

Any G0 failure is evidence about the production architecture and becomes an implementation priority. Do not backfill missing semantics in Python and do not alter the preregistered protocol to make the arm pass.

## Claim boundary

Passing this contract would prove only that G0 is an honest Graphene-only execution arm suitable for an architecture ablation. It would **not** prove that GrapheneDB improves truth acquisition, prevents false convergence, or outperforms B0/G1/G2. Those are empirical questions for the later frozen score-bearing run.
