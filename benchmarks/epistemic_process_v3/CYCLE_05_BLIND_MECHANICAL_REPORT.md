# EP-PROCESS-V3 — Cycle 5 Blind Mechanical Campaign

Status: **IN PROGRESS — AWAITING AUTHORITATIVE CAMPAIGN**

## Objective

Execute the revised Cycle-4 runtime across all 384 frozen V3 episodes and all five preregistered profiles while preserving a strict blind boundary.

This cycle validates execution mechanics, telemetry, completeness, determinism and mechanism activation only.

It must not evaluate whether any answer is correct.

## Frozen Inputs

Task candidate:

`sha256:575c89930859bc6f23ecd218c7ed4c27032ec3a9f5a8c12bea1bbe84ab356829`

Frozen reasoner-safe runtime view:

`sha256:d298532080f8235ac19468a9a236792d56a386444e0bddeaf54dccff7746a91b`

All Cycle-2 semantic/metric/claim/statistical contracts remain hash-protected.

## Blind Input Boundary

Cycle 5 constructs two inputs:

1. reasoner-safe visible runtime tasks;
2. harness-only latent graph fixtures.

The harness fixture contains latent structural evidence but no:

- task-family labels;
- oracle;
- expected answer;
- correctness;
- expected status;
- expected revision;
- expected challenge;
- expected usefulness;
- claim labels.

The harness environment is used only to seed hidden graph structure.

## Configurations

Every episode executes:

- C0
- G0E
- C1
- G1
- G2

Total planned configuration executions:

**1,920**

C0 and C1 use the unchanged V2 common decision-head function.

G0E/G1/G2 execute through the production Graphene/HypoKosh runtime.

## Required Mechanical Gate

Cycle 5 passes only if:

- 384 episodes execute;
- 1,920 configuration-episodes exist;
- every configuration has exactly 384 episodes;
- no runtime failure occurs;
- no telemetry gap occurs;
- runtime step counts match sanitized visible input;
- Challenge is exercised on G2;
- CorroborationSearch is exercised;
- opportunity-gated Reopen is exercised;
- positive expansion opportunities are observed;
- at least one recovery round changes the frontier;
- at least one latent G2 episode actually discovers harness-seeded latent evidence;
- 24 blindly selected episodes replay byte/semantically identically;
- score authorization remains false;
- no oracle join occurs;
- no outcome metric is calculated;
- raw traces are deleted before artifact upload.

## Artifact Boundary

The permanent artifact may contain only:

- mechanical summary;
- input receipt/hashes;
- source hashes;
- commit/tree identity.

It must not contain:

- raw episode traces;
- expected outcomes;
- accuracy;
- correctness;
- score;
- claim evaluation.

## Gate

**INCOMPLETE — campaign has not run yet**

## Next Authorized Activity

If and only if Cycle 5 passes:

**V3 Cycle 6 — freeze the evaluator/statistical score boundary against the already frozen candidate and implementation, still without scoring.**
