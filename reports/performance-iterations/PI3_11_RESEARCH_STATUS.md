# PI3.11 canonical research status

Date: 2026-08-19

This is a **status/evidence record only**. It does not mean the complete canonical PI3.11 source tree has been materialized into this GitHub branch. PR #20 remains a draft reconciliation PR and must not be merged as the complete implementation.

## Official LoCoMo benchmark boundary

PI3.11 verified the original LoCoMo implementation and freezes retrieval isolation to the current sample/conversation (`sample_id`). Required invariants are:
- evidence identity: `(conversation_index, dia_id)`
- candidate set: current conversation/sample only
- scoreable questions: 1,977
- no temporal adjacency relabelled as causal
- no QA answer/category/evidence IDs as router input features

## Additive multi-channel correction

The first PI3.11 router treated message, observation and session channels as mutually exclusive alternatives. That is not the intended GrapheneDB architecture. The sealed experiment uses an additive cascade:

`M -> M ∪ O -> M ∪ O ∪ S`

A deeper retrieval stage never discards evidence already found by an earlier stage.

### Full-corpus native GrapheneDB evidence

Actual GrapheneDB over all 1,977 questions:
- message text: **60.0405%**
- observation/provenance standalone: **69.9039%**
- session hierarchy standalone: **76.8336%**
- additive `M ∪ O`: **72.5341%**
- additive `M ∪ O ∪ S`: **81.6388% evidence reach**
- temporal-as-causal edges: **0**

### Untouched held-out conversations 5-9

Train/tune conversations 0-4: 996 questions. Held-out conversations 5-9: 981 questions.

- `M`: **61.6718%**
- `M ∪ O`: **74.2100%**
- `M ∪ O ∪ S`: **83.2824% evidence reach**
- observation/provenance rescues **123** held-out message misses
- session hierarchy rescues a further **89** misses after `M ∪ O`

The 81.6388% and 83.2824% values are **evidence reachability**, not answer accuracy and not a claim that the final ranked top-10 contains all expanded evidence.

## Adaptive router decision

The router chooses an early stopping depth but does not yet predict safe early exit reliably.

Held-out 97%-of-full policy:
- reach: **81.14%** vs 83.28% always-full
- quality loss: **2.14 percentage points**
- indexed-node saving: **2.83%**
- source-candidate saving: **14.26%**
- paired-query 95% CI for quality delta: **[-3.06 pp, -1.33 pp]**

Held-out 99%-of-full policy:
- reach: **82.57%**
- quality loss: **0.71 pp**
- indexed-node saving: **0.99%**
- paired-query 95% CI: **[-1.33 pp, -0.20 pp]**

At the 99.5%-of-full target, quality matches 83.28% only by activating the complete hierarchy for ~99.8% of held-out queries, producing essentially zero savings.

Decision: **current learned query-adaptive router REJECTED for production promotion.**

## Neural semantic channel

Real 384D `all-MiniLM-L6-v2`, official per-conversation candidate scope:
- Hit@10: **36.72%**
- MRR@10: **0.177662**
- 1,977 queries
- QA labels used for embedding: false

MiniLM is an independent semantic signal but remains weaker standalone than GrapheneDB's deterministic text channel. A per-query semantic-assisted router supplemental was not completed and is not counted. No router-improvement claim is inferred from aggregate MiniLM metrics.

## Product and causal integrity

Fresh sealed acceptance:
- permanent PI3.11 additive gate: **PASS**
- additive multi-channel research: **ACCEPTED**
- adaptive router production promotion: **REJECTED**
- product regression: **68/68 PASS**, 0 failures, 26.34s
- causal CSuite: **15/15 correct roots**
- authoritative shadow mismatches: **0**
- shadow top-1 agreement: **100%**
- mean/worst Recall@k: **1.000 / 1.000**
- production `src/` / `include/` changes: **none**

## Sealed local artifacts

Canonical local PI3.11 commit:
`5423183e1cca104ee902ceb0e1ac02718982e317`

SHA-256:
- source ZIP: `f6d2be9ac4b7d567526e3a9ec478d9f000943ef9d33138e54456d66fea064411`
- evidence ZIP: `f594696f6e4ae523ef01c57448161aeb5addf02751990b2a67eef6101141fafd`
- PI3.10 -> PI3.11 patch: `b1281a7c655aa204737406975d1bfd1c5a48032712ffd72e8d9ae89cd7f53de6`

These identify validated local artifacts; they are not GitHub tree hashes.

## Final decision

- official benchmark-scope correction: **ACCEPTED**
- additive multi-channel retrieval: **ACCEPTED as research evidence**
- full-corpus native 81.64% structural evidence reach: **ACCEPTED**
- held-out 83.28% structural evidence reach: **ACCEPTED**
- neural semantic standalone evidence: **ACCEPTED as measurement**
- current learned adaptive router: **REJECTED for production**
- production retrieval policy: **UNCHANGED**
- PI3.11 research increment overall: **ACCEPTED**
