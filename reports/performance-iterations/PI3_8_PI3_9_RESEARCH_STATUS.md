# PI3.8 / PI3.9 / PI3.10 canonical research status

Date: 2026-08-19

This file records evidence from the canonical local GrapheneDB research line. It does **not** mean the corresponding source tree has been materialized into this GitHub branch.

## Remote materialization boundary

PR #20 remains a draft reconciliation PR. The accepted canonical source line is still ahead of remote `master`, and the complete source tree has not been atomically synchronized through the available connector. Do not merge this PR as the complete implementation.

## PI3.10 benchmark correction

PI3.10 found a material benchmark-scoring defect in the earlier LoCoMo runs: LoCoMo `dia_id` strings repeat across conversations, but PI3.6-PI3.9 scorers used one global `dia_id -> node_id` map. Later conversations therefore overwrote evidence mappings for earlier conversations.

Measured corpus identity facts:
- messages with scoped dialogue IDs: 5,882
- unique raw `dia_id` strings: 1,033
- raw IDs reused across conversations: 871
- scoreable questions with `(conversation_index, dia_id)` evidence: 1,977

Therefore the historical PI3.6-PI3.9 cross-conversation LoCoMo evidence metrics are **retracted**. Architecture, product-regression, CSuite and Exact-vs-shadow evidence that did not depend on that faulty LoCoMo mapping is unaffected.

## Corrected PI3.7 text baseline

The corrected full-corpus PI3.7 1536D word+character hashed TF-IDF scorer gives:
- Hit@1: 0.26454223571067276
- Hit@5: 0.4587759231158321
- Hit@10: 0.5336368234699039
- MRR@10: 0.35142652407447555

The ±1 conversational-context representation gives Hit@10 0.5998988366211432, with lower Hit@1 (0.16843702579666162).

The previous ~7.99% PI3.7 full-corpus Hit@10 figure is invalid and must not be used.

## Corrected PI3.8 MiniLM closeout

The real 384D `all-MiniLM-L6-v2` model was rerun with conversation-scoped evidence:
- Hit@1: 0.09155285786545271
- Hit@5: 0.2276176024279211
- Hit@10: 0.3095599393019727
- MRR@10: 0.15021557434304006

Decision remains directionally **PI3.8 target-quality FAIL**: MiniLM alone does not beat the corrected PI3.7 text baseline, so neural-only promotion is still not justified. The previous MiniLM magnitude (~7.49% Hit@10) is retracted and replaced by the corrected value above.

## PI3.9 fusion status

The previous RRF60 metrics and the PI3.9 fusion acceptance decision are **retracted/reopened** because that evaluation used the invalid global evidence mapping.

Do not cite the old RRF60 9.155% Hit@10 result as valid evidence. Fixed lexical+MiniLM fusion must be rescored with `(conversation_index, dia_id)` evidence before any fusion conclusion is restored.

## PI3.10 corrected multi-channel experiment

PI3.10 evaluates GrapheneDB as a multi-channel memory system rather than a single vector retriever. The corrected structural graph contains:
- 5,882 dialogue-message nodes
- 2,526 LoCoMo observation/fact nodes
- 272 session-summary nodes
- 5,052 provenance/support edges
- **0 temporal-as-causal edges**

QA answers/evidence labels are not used to construct the graph or fit representations.

Corrected evidence results:
- message-only text Hit@10: **53.36%**
- message + observation provenance evidence reachability from top-10 anchors: **64.44%**
- mean source-message candidates in that arm: **9.12**
- matched-budget pure message similarity: **52.91%**
- observation provenance rescues **277/922 = 30.04%** of message-top10 misses
- message + observation + session-summary evidence reachability from top-10 anchors: **71.37%**
- matched-budget pure message similarity for that larger arm: **61.20%**
- hierarchy rescues **427/922 = 46.31%** of message-top10 misses

The 71.37% number is evidence reachability after provenance expansion, **not** a final ranked Hit@10 claim; that arm reaches about 33 source messages on average.

A native GrapheneDB first-50 parity run exactly reproduced the optimized evaluator: **54% -> 68% -> 78%** across message-only, observation-provenance, and summary-provenance arms.

## Causal channel and product regression

PI3.10 does not relabel LoCoMo temporal adjacency as causal. True causal retrieval continues to be evaluated on Microsoft CSuite, where the graph is supplied as ground truth.

Fresh PI3.10 acceptance:
- full Release product regression: **68/68 PASS**, 0 failures, 15.56s CTest real time
- CSuite: **15/15 correct causal roots**
- authoritative shadow mismatches: 0
- shadow top-1 agreement: 100%
- shadow mean/worst Recall@k: 1.000 / 1.000
- governed reasoning remains `evidence_required`
- PI3.10 production changes under `src/` or `include/`: **none**

## Sealed PI3.10 local artifacts

Local sealed PI3.10 reconstruction commit: `e391f3368dab17af34339778f6823e073091573a`

- source ZIP SHA-256: `f4cb8b3c2b25b664ffed8cafac0048afeb384acdc02db0a3f288d35aabf23eca`
- evidence ZIP SHA-256: `694d6af1cb33a81c1339a7a9aad07847a3e0329077a8e29f1f8711233fa4a4f8`
- PI3.9 -> PI3.10 patch SHA-256: `bfc3c08c3d8265f4174c2f95984a3d35296224036923b35e39acfafa77f7a6c8`

These hashes identify validated local artifacts; they are not GitHub tree hashes.

## Authority boundary

Nothing in PI3.10 approves:
- `VectorIndexKind::Auto` changes;
- CLI/server default changes;
- durable neural/shadow formats;
- replacing Exact Flat authority;
- causal/Hex authority changes;
- HypoKosh/FiberBundle governance changes;
- answer authority for semantic/provenance candidate sources.

The next retrieval work should use corrected evidence identity and should keep lexical, neural semantic, provenance/temporal, and causal channels separately observable rather than collapsing them into one ungrounded score.