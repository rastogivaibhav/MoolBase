# PI4.1 Research Status

**Research result: ACCEPTED. Production reasoning policy: UNCHANGED / NOT PROMOTED.**

PI4.1 moves the reasoning line from controlled mechanism tests to answer-level truth classification on a frozen external rule-reasoning protocol.

## Scope boundary

The quantitative corpus is a deterministic **RuleTaker-style open-world grounded Horn holdout**, not the official RuleTaker downloadable test set and not a leaderboard score.

- 1,200 total cases
- 1,125 True/False/Unknown cases
- 75 contested contradiction cases
- proof depths 0–5
- multi-premise rules, distractors, explicit polarity
- 432 tagged multi-fibre cases
- independent fixed-point oracle creates evaluator labels before native execution
- labels are not stored in GrapheneDB or used for traversal

An actual external smoke case reproduces the published `allenai/ruletaker` README example (`nice(lion)`, gold `true`, minimum proof depth 2).

## Answer-level result

- P1 one-pass: **525/1125 = 46.67%**
- I3 three-pass: **825/1125 = 73.33%**
- I5 five-pass: **1125/1125 = 100.00%**
- I3 improvement over P1: **+26.67 pp**
- I5 improvement over P1: **+53.33 pp**
- P1 misses: **600**
- I3 rescues: **300/600 = 50%**
- I5 rescues: **600/600 = 100%**
- false promotions at I5: **0**

By proof depth, I5 is 100% at depths 0–5. I3 is 100% through depth 3 and does not claim to solve depth 4/5 within a three-pass budget.

## Contradiction / fibres

- contested detection I3: **75/75**
- contested detection I5: **75/75**
- complete admitted-fibre inspection: **432/432 multi-fibre cases**

The harness contains no first-success shortcut: every admitted candidate rule is inspected even after one proof establishes the same conclusion.

## Published RuleTaker sample

- gold: `true`
- published proof depth: 2
- P1: `unknown`
- I3: `true`
- I5: `true`

This is a smoke case only, not an official aggregate benchmark claim.

## Hex/lattice boundary

PI4.0 remains the coordinate-driven physical-Hex neighbour result. PI4.1 isolates answer-level iterative inference and gates rule admission through GrapheneDB `lattice_neighbors()` over explicit same-layer synthetic lattice bonds plus `Supports`/`Mechanistic` structure. It does not replace or inflate the PI4.0 physical-layout claim.

## Permanent gate / regression

- PI4.1 permanent gate: **PASS**
- corpus regeneration byte-identical: PASS
- two native result JSON runs byte-identical: PASS
- complete product CTest: **68/68 PASS**, 0 failures
- architecture-preservation subset: **10/10 PASS**
- Microsoft CSuite: **15/15 structural causal roots**
- governed CSuite reasoning remains `evidence_required`
- authoritative shadow mismatches: **0**
- shadow top-1 agreement: 100%
- mean/worst Recall@k: 1.000/1.000
- PI4.0→PI4.1 production `src/`/`include/` delta: **0 bytes**

A full all-target build still reaches a pre-existing `examples/api_coding_memory.cpp` missing-field-initializer warning under `-Werror`; that example source is unchanged from sealed PI4.0. Required product/test/runtime targets build and all 68 registered tests pass.

## Seal

Original prior canonical PI4.0 local commit recorded by the previous iteration:
`c9374a8a628d64a0674ee75e6b92279d46283bbf`

The PI4.1 execution environment was reconstructed from the sealed PI4.0 source ZIP, which contains no `.git` objects. The PI4.1 Git seal therefore uses reconstructed local ancestry and is not claimed to be a direct Git child object of `c9374a8`.

Reconstructed PI4.1 seal commit:
`d404e82d055ecce289672eda1e3f25a609a3d77b`

Artifact SHA-256:
- source ZIP: `8e4f1343c0e3a0305fad73dadc8a3aa196b4e5180353d33fb6c1753d5d1b5063`
- evidence ZIP: `e23737dcfb6b407f9e3b2db39ce10365bf316942cc06460e2d548f133b382084`
- PI4.0→PI4.1 patch: `089ac154a2066eeb03839a6f63e8ee0e1948905fc705144f9deacfb297dac242`

## Authority boundary

Nothing in PI4.1 approves production minimum-round changes, answer authority for the research harness, Exact Flat replacement, causal/Hex authority changes, HypoKosh/FiberBundle governance changes, natural-language semantic-parser claims, or any official RuleTaker leaderboard claim.

**Next evidence boundary:** substantial official external-file evaluation and same-model one-pass vs iterative natural-language answer comparison.