# G0 production path design — GrapheneDB without HypoKosh or DWM

Status: implementation contract for #53/#51. This document does not change Epistemic Process v1 tasks, event order, scoring semantics, evidence-family/dependency labels, or any frozen/score-bearing artifact.

## Purpose

G0 exists to answer one falsifiable architecture question:

> What does GrapheneDB's persistent evidence/provenance layer contribute before HypoKosh competing-hypothesis reasoning and DWM challenge/reopen/synthesis are enabled?

A valid G0 must therefore execute real GrapheneDB storage/evidence/provenance code while never constructing or executing HypoKosh or DWM mechanisms. Suppressing their receipt events after execution is invalid.

## Current source boundary

`RuntimeOptions` now declares `enable_hypokosh` and `enable_dwm`, and `ReasoningReceipt` separates declared capability from observed execution. `CompleteHypoKoshRuntime::reason(...)` also enforces `DWM => HypoKosh` and currently fails closed with `Graphene-only G0 execution is not implemented yet` when HypoKosh is disabled.

That failure is scientifically preferable to a false ablation. The next change must replace only that explicit G0 failure with a production-owned Graphene path.

## Required execution topology

```text
observations
    |
    v
GrapheneDB durable graph / snapshot
    |
    +--> source + derivation + evidence-family provenance
    |
    +--> Graphene-owned admissible evidence projection
    |
    v
G0 governed projection + native receipt

HypoKosh: NOT CONSTRUCTED / NOT EXECUTED
DWM:      NOT CONSTRUCTED / NOT EXECUTED
```

G0 is not allowed to call the benchmark evaluator or reproduce its oracle helpers.

## Implementation rule

Prefer a dedicated production function/class whose ownership is Graphene core, for example a `GrapheneEvidenceRuntime` or equivalent internal path. `CompleteHypoKoshRuntime::reason(...)` may delegate to it when `enable_hypokosh == false && enable_dwm == false`.

The Graphene-only path may use existing storage/provenance/query primitives that belong to Graphene core. It must not instantiate or call mechanisms whose semantics constitute the ablated layers, including competing-hypothesis selection or DWM opposition/reopen/synthesis.

If an existing helper mixes Graphene retrieval with HypoKosh/DWM semantics, split the reusable Graphene primitive out rather than calling the mixed helper and hiding its output.

## Receipt contract

A successful G0 receipt must make the absence of higher layers positive, machine-checkable evidence:

```text
hypokosh_capability_enabled = false
dwm_capability_enabled      = false
graphene_executed           = true
opposition_executed         = false
```

In addition:

- no event with `source == HypoKosh` may be emitted;
- no event with `source == DialecticalModelWorlds` may be emitted;
- no challenge, reopen, or revision event may be synthesized by the adapter;
- evidence/provenance references must come from runtime state, not evaluator metadata;
- disabled layers must remain distinguishable from layers that executed but happened to emit no transition.

A Graphene-core terminal/projection event is permitted only if it describes what the Graphene-only path actually produced. It must not masquerade as a HypoKosh hypothesis decision.

## Anti-oracle boundary

The system-under-test must never receive or consume:

- `terminal_supported`;
- `decisive` as an answer hint;
- expected terminal answer/state;
- evaluator scoring outputs;
- duplicated forms of `run_v1.py::_choose`, `_is_refuted`, or `_independent_support`.

Preregistered provenance/dependency information that is part of the observation/runtime input remains allowed exactly as specified by Epistemic Process v1; scoring-only labels do not.

## Tests required before #51 may run G0

### 1. Invalid dependency

`enable_hypokosh=false, enable_dwm=true` must fail explicitly.

### 2. Genuine G0 execution

`enable_hypokosh=false, enable_dwm=false` must execute successfully on a deterministic fixture and assert:

- `graphene_executed == true`;
- `hypokosh_capability_enabled == false`;
- `dwm_capability_enabled == false`;
- `opposition_executed == false`;
- zero HypoKosh events;
- zero DWM events.

### 3. Instrumented non-execution

Where practical, add a test seam/counter proving HypoKosh/DWM constructors or entry points are not invoked on G0. Event absence alone is insufficient evidence.

### 4. Default compatibility

Default `RuntimeOptions{}` must retain current production behavior and existing tests must remain green.

### 5. Leakage test

The production runner must pass when scoring-only fields are absent from the runtime input and must fail the gate if code attempts to consume them.

## First clean-checkout experiment after implementation

Run B0/G0/G1/G2 from a clean checkout in **UNSCORED** mode only.

Preserve for every episode/configuration:

- raw runtime events;
- native receipt;
- runtime/adapter/schema failures;
- timeouts;
- repo commit;
- compiler/OS/runtime identity;
- task/protocol/evaluator/adapter/runner hashes;
- deterministic seed statement.

Do not remove failing episodes. Do not tune against the UNSCORED output and then score the same protocol as if untouched.

Only after the complete UNSCORED run is inspected should a dedicated freeze change hash and lock the protocol for the first score-bearing architecture ablation.

## How results become implementation priorities

The first score-bearing G0/G1/G2 result is allowed to be negative.

- If G0 adds no value over B0, inspect whether persistence/provenance affects any preregistered process metric before adding features.
- If G1 does not improve evidence use or false-convergence resistance over G0, HypoKosh's competition mechanism becomes the implementation target.
- If G2 does not improve refutation response/revision inertia over G1, DWM challenge/reopen becomes the implementation target.
- If a layer improves safety only by abstaining more, report selective coverage with accuracy; do not call it a reasoning win.
- If receipt completeness fails, fix observability before interpreting missing transitions as mechanism failure.

## External relevance

Recent public work makes this separation more important, not less. Jev/Jev-Mem occupy fast typed decisions and memory control; ENGRAM and recent provenance/belief-revision work occupy persistent provenance and revision. GrapheneDB therefore needs layer-by-layer evidence that its integrated architecture earns convergence rather than merely possessing the same nouns.

This contract intentionally avoids a competitive claim. It defines what must be true before GrapheneDB can make one.

Relates to #25, #40, #51, #53 and #55.
