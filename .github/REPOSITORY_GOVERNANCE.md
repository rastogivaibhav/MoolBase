# Repository governance

This document records the intended source/release governance for the public MoolBase developer preview.

It does **not** prove that GitHub branch or tag protection is currently enabled. The live GitHub settings remain authoritative. Owner-setting status is tracked in issue #43.

## Default branch

The default branch is `master`.

Intended controls:

- changes flow through pull requests;
- force-push is blocked;
- deletion is blocked;
- normal core/release CI checks must pass before merge;
- the solo-maintainer workflow must not require an external reviewer if that would make maintenance impossible.

Direct emergency changes, if ever required, should be exceptional, documented, and followed by the same validation gates.

## Release and frozen history

Published and scientifically frozen history should be treated as immutable.

Intended protected patterns:

- tags: `v*`
- branches: `release/*`
- branches: `freeze/*`

These refs should block deletion and non-fast-forward/force updates.

Historical research branches may remain unprotected unless they are explicitly designated as a frozen protocol or release source.

## Release integrity

A public MoolBase distribution should preserve:

- exact source commit identity;
- Apache-2.0 `LICENSE`;
- `NOTICE`;
- `THIRD_PARTY_NOTICES.md`;
- source-bound manifest;
- SHA-256 checksum;
- SPDX SBOM where produced by the release workflow;
- build/SBOM attestation where the GitHub workflow/platform supports it.

Checksums establish integrity, not enterprise readiness.

## Scientific evidence

Frozen benchmarks, preregistrations, task universes, receipts and score-bearing artifacts must not be rewritten to make a later implementation appear better.

If semantics change, create a new protocol/version and preserve the historical result.

## Current public architecture

MoolBase currently presents the following stack:

1. **MoolBase Core** — durable evidence/provenance and causal-memory storage.
2. **Dense hexagonal lattice topology** — durable axial coordinates, validated bonds and optional lattice-aware retrieval.
3. **FiberBundle** — deterministic target/role/lineage-aware evidence projection.
4. **HypoKosh / Hypothesis Engine** — competing hypotheses and governed convergence/abstention.
5. **DWM / Dialectic Engine** — material opposition, challenge, bounded reopen/re-expansion and revision.
6. **Epistemic receipts** — inspectable machine-readable decision provenance.

Historical `GrapheneDB`, `graphene`, `GRAPHENEDB_*`, HypoKosh and DWM identifiers remain part of the compatibility/research lineage.

## Owner-held controls

Some controls cannot be established by repository contents alone:

- GitHub repository description/homepage/topics;
- branch/tag rulesets;
- correction of already-published release text;
- rights-holder certification.

These are tracked explicitly in issues #43 and #44 rather than being implied by documentation.
