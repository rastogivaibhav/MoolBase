# PI3.11 canonical research status

Date: 2026-08-19

This is a **status/evidence record only**. It does not mean the complete canonical PI3.11 source tree has been materialized into this GitHub branch. PR #20 remains a draft reconciliation PR and must not be merged as the complete implementation.

## Official LoCoMo scope correction

PI3.11 verified the original LoCoMo evaluation implementation and freezes retrieval candidate isolation to the current sample/conversation (`sample_id`). PI3.10 had corrected evidence identity but still searched all ten independent conversations as a global store; that remains a harder stress experiment, not the official benchmark protocol.

Required PI3.11 benchmark invariants:
- evidence identity: `(conversation_index, dia_id)`
- candidate set: current conversation/sample only
- scoreable questions: 1,977
- no temporal adjacency relabelled as causal
- no QA answer/category/evidence IDs as router input features

## Native full-corpus multi-channel result

Actual GrapheneDB, all 1,977 questions:
- message text ranked Hit@10: **60.04%**
- message + observation/provenance evidence reach: **69.90%**
- message + observation + session hierarchy evidence reach: **76.83%**
- observation rescues of message-top10 misses: **247 / 790 = 31.27%**
- hierarchy rescues: **413 / 790 = 52.28%**
- temporal-as-causal edges: **0**

The 69.90% and 76.83% values are evidence reachability after bounded provenance expansion from ten ranked anchors, not final answer accuracy or final ranked Hit@10.

## Held-out adaptive routing

Frozen split:
- train/tune conversations 0-4: 996 questions
- untouched test conversations 5-9: 981 questions

Fixed held-out test:
- message: **61.67%**
- observation/provenance: **71.66%**
- always-full hierarchy: **77.06%**

Learned 97%-of-full training target on held-out test:
- evidence reach: **74.72%**
- mean structural nodes scored: **871.14** vs **917.95** always-full
- source candidates: **33.58** vs **38.23**

Decision: **current learned router REJECTED for production promotion**. The quality loss is too large for the achieved cost saving.

A non-deployable routing oracle demonstrates headroom:
- held-out evidence reach: **83.28%**
- mean structural nodes: **684.27**
- mean source candidates: **13.91**

This is architecture headroom only, not a router claim.

## Neural semantic channel

Real 384D `all-MiniLM-L6-v2`, official per-conversation candidate scope:
- Hit@10: **36.72%**
- MRR@10: **0.177662**
- 1,977 queries
- QA labels used for embedding: false

MiniLM remains weaker standalone than the deterministic text channel and is not a replacement candidate.

A planned per-query lexical+MiniLM fusion / semantic-assisted-router supplemental is **not counted**. The completed remote per-query evidence could not be transferred into the local execution sandbox; Render then reached its Hobby service-count limit; and the temporary GitHub Actions evidence-fetch fallback failed before any job step executed. The temporary workflow was removed afterwards. No fusion result is inferred from aggregate scores.

## Product and causal integrity

Fresh acceptance evidence:
- permanent PI3.11 gate: **PASS**
- router production-promotion assertion: **REJECTED**
- product regression: **68/68 PASS**, 0 failures
- causal CSuite: **15/15 correct roots**
- authoritative shadow mismatches: 0
- shadow top-1 agreement: 100%
- mean/worst Recall@k: 1.000 / 1.000
- production `src/` / `include/` changes: **none**

## Sealed local artifacts

Canonical local PI3.11 commit:
`74c5ffab60d7fcfc75d75dac9fc7db0c537f6206`

SHA-256:
- source ZIP: `f1f82030c5fb3366b31e5a40d6b19b4431e170ef0d3e5b184cd7b14c5b67e26d`
- evidence ZIP: `6c44313a68b0b1b2d263a4f8aab00e57b3712a65ecfc876a1e597fb707aedc8a`
- PI3.10 -> PI3.11 patch: `6f55ee11d16d9b2e6c377535065955c116e824833a08034bf5defbdb5943a00b`

These identify validated local artifacts; they are not GitHub tree hashes.

## Final decision

- official benchmark-scope correction: **ACCEPTED**
- native multi-channel evidence: **ACCEPTED**
- neural semantic standalone evidence: **ACCEPTED as measurement**
- query-adaptive routing feasibility: **DEMONSTRATED**
- current learned router: **REJECTED for production**
- production retrieval policy: **UNCHANGED**
- PI3.11 research increment overall: **ACCEPTED**
