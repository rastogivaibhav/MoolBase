# PI4.0 iterative Hex × Fiber reasoning research status

Date: 2026-08-19

This is a **status/evidence record only**. It does not mean the complete canonical PI4.0 source tree has been materialized into this GitHub branch. PR #20 remains a draft reconciliation PR and must not be merged as the complete implementation.

## Research question

PI4.0 tests the reasoning requirement that GrapheneDB must not grant authority to the first plausible conclusion. A reasoning episode should iteratively reopen the knowledge structure, explore distinct Hex neighbourhood combinations, traverse every path in every admitted FiberBundle, expose opposition/counterevidence, and converge only after a minimum challenge depth or terminate unresolved.

The frozen controlled policy is:

- minimum reasoning rounds: **3**;
- maximum reasoning rounds: **5**;
- evidence/path accumulation across rounds is additive;
- at least **3 genuinely distinct admitted Hex/reopen seed sets** are required;
- every path in every final admitted FiberBundle must be inspected;
- hidden neighbour evidence is made semantically distant so neighbour rescue cannot be attributed to a larger vector top-k;
- confidence alone is not a valid stop condition;
- budget exhaustion does not force an answer.

## Native controlled benchmark

30 repetitions × 6 scenario families = **180 cases**.

Scenario families:
1. stable conclusion;
2. complete-fibre rescue;
3. one-hop neighbour rescue;
4. two-hop/deep-neighbour rescue;
5. deliberately premature convergence;
6. late two-hop counterevidence.

Final results:

- minimum-three-round enforcement: **100%**;
- complete final admitted-FiberBundle traversal: **100%**;
- final desired controlled outcome: **100%**;
- mean reasoning rounds: **4.17**;
- mean distinct admitted Hex/reopen sets: **3.17**;
- neighbour rescue: **100%**;
- complete-fibre rescue: **100%**;
- deliberately premature convergence detected: **100%**;
- premature convergence corrected/contested after iterative challenge: **100%**;
- late two-hop counterevidence challenge: **100%**;
- reasoning rescue over all controlled cases: **66.67%**.

Desired controlled outcome by round:

- round 1: **33.33%**;
- round 2: **83.33%**;
- round 3: **100%**.

Neighbour-dependent correctness by round:

- round 1: **0%**;
- round 2: **50%**;
- round 3: **100%**.

The two-hop hidden evidence is deliberately semantically distant. Its round-three recovery is therefore controlled evidence for explicit physical Hex-neighbour reopening rather than semantic retrieval leakage.

Two benchmark executions produced byte-identical JSON evidence.

## Interpretation

PI4.0 supports the user's iterative reasoning requirement **as a controlled mechanism result**: first-pass authority is inadequate in these frozen worlds, complete fibre traversal can rescue provisional reasoning, and repeated Hex-neighbour challenge can both recover missing support and overturn a misleading early conclusion.

It does **not** establish that exactly three rounds is universally optimal, nor does it establish external natural-language answer superiority, autonomous causal discovery, scientific hypothesis invention or real-world causal identification.

## Product / architecture integrity

Fresh evidence:

- permanent PI4.0 gate: **PASS**;
- product regression: **68/68 PASS**, 0 failures;
- architecture-preservation subset: **10/10 PASS**;
- Microsoft CSuite structural causal roots: **15/15**;
- authoritative shadow mismatches: **0**;
- no PI4.0 changes under production `src/` or `include/`.

A fresh from-scratch full Release rebuild was attempted but exceeded the execution harness's 300-second tool-call ceiling during compilation without an observed compiler error. Because PI4.0 changes no production `src/` or `include/` files, fresh CTest execution used the already-built accepted PI3.12 production binaries. The new PI4.0 native reasoning benchmark target itself was freshly compiled from the sealed PI4.0 source tree and passed its permanent gate.

## Sealed local artifacts

Reconstructed PI3.12 baseline commit:
`fbb38f0460c0db206cfd3b71d3119625f4154fe1`

Canonical local PI4.0 commit:
`c9374a8a628d64a0674ee75e6b92279d46283bbf`

PI3.12 → PI4.0 delta:
- 6 research/benchmark files;
- 1,238 insertions;
- 0 deletions;
- production `src/` / `include/` changes: **none**.

SHA-256:
- source ZIP: `46d0bdb2a8af6c4f576e31985298be62176b89122e9c66bcb38d0d379a3d95bc`;
- evidence ZIP: `bfd6953a6d87a849036864ab777cd11bca2a82f9c848b491480226c41bbda410`;
- PI3.12 → PI4.0 patch: `8bf37e014aa5960c8f1ec1defda221039d9c6dce96750c327e593f7f66d82f80`.

## Decision

- iterative Hex × Fiber controlled mechanism: **ACCEPTED**;
- minimum-three-round research policy: **SUPPORTED on the frozen controlled worlds**;
- complete admitted FiberBundle traversal: **SUPPORTED**;
- first-answer authority in PI4 reasoning research: **REJECTED**;
- default production runtime policy change: **NOT YET APPROVED**;
- next required evidence: external answer-level iterative reasoning benchmark using the same evidence/model across one-pass and iterative arms.
