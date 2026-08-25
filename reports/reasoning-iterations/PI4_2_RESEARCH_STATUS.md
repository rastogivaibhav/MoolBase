# PI4.2 Research Status

Date: 2026-08-20

## Decision

- GrapheneDB-native natural-language context/evidence pipeline: **PASS / ACCEPTED**.
- Exact pinned fixed-model natural-language comparison: **BLOCKED / UNSCORED**.
- Same-model natural-language superiority: **NOT CLAIMED**.
- Production reasoning defaults: **UNCHANGED / NOT APPROVED**.

## Frozen experiment

Four answer-model arms are frozen over the same natural-language facts/rules/question:

`RAW -> G1 -> G3 -> G5C`

where GrapheneDB evidence is additive and G5C adds an opposition audit without exposing the evaluator label.

Aggregate selection: **120 cases** (30 true, 30 false, 30 unknown, 30 contested) plus one published RuleTaker README smoke case outside the aggregate.

Prompt/label isolation:
- label-free prompt JSONL SHA-256: `833cac63e7c55f5fe227d6b269dd517e37fb664b0c6d7223702751657a003eb9`;
- local evaluator-label JSONL SHA-256: `cdb4e34a4e270bab5f9370b9bcd7db8c90de5e79d8c844adbe4b4c3b2d6ddd6c`;
- two fresh exporter runs byte-identical;
- native G5C symbolic state matches the independent evaluator on **120/120** aggregate cases.

Native structural pre-model classifications:
- G1: **50/120 = 41.67%**;
- G3: **100/120 = 83.33%**;
- G5C: **120/120 = 100%**.

These are not answer-model accuracy and are not an official RuleTaker score.

## Fixed model boundary

Frozen before scoring:
- `unsloth/SmolLM2-360M-Instruct-GGUF`;
- `SmolLM2-360M-Instruct-Q4_K_M.gguf`;
- expected SHA-256 `16c7f1667fea34bacad196a57b548effcb37614db4ab5677a20c8c7b823b9e63`;
- llama.cpp `9ee9fc04c136ef2ae729bfc60d18961b23c13ddf`;
- temperature 0, seed 4242, max output 8 tokens.

The exact model was not executed. A fresh GitHub Actions transfer run (`32336024770`, job `96325655022`) failed before any step executed. A no-cost Render executor then could not be created because the Hobby workspace had reached its **25 active service** limit. No paid upgrade was authorized and no alternative model was substituted.

Therefore model-utility gates remain **BLOCKED**, not PASS/FAIL.

## Fresh integrity evidence

- permanent PI4.2 native/context gate: **PASS**;
- fixed-model utility gate: **BLOCKED**;
- production promotion: **NOT APPROVED**;
- complete CTest: **68/68 PASS**, 0 failures, final rerun 14.86s;
- architecture subset: **10/10 PASS**;
- architecture fingerprint: `a916f5192c2e89db79e0c10a81c199322f7821acf390adf1429eee3ad212775c`;
- Microsoft CSuite structural roots: **15/15**;
- governed CSuite reasons: `evidence_required`;
- authoritative shadow mismatches: **0**;
- shadow top-1 agreement: **100%**;
- mean/worst Recall@k: **1.000 / 1.000**;
- PI4.1 -> PI4.2 production `src/`/`include/` delta: **0 bytes**.

## Seal

Original prior PI4.1 canonical local commit:
`d404e82d055ecce289672eda1e3f25a609a3d77b`

PI4.2 was reconstructed from the sealed PI4.1 source ZIP and therefore does not claim direct Git-object ancestry from that commit.

Reconstructed PI4.2 seal commit:
`0054975afb4b1b8dfdf5fc15cbeb7e5b10b01716`

PI4.1 -> PI4.2 delta:
- **12 files changed**;
- **1,214 insertions**;
- **0 deletions**;
- production `src/` / `include/`: **0 bytes changed**.

Artifact SHA-256:
- source ZIP: `aa327ecf8e958b4fba5aa8d38a4fb5ce04a0b511ffd20c1f95a2902e32269938`;
- evidence ZIP: `66933ee94041632a1f73332981add77ca56c3596644c6e7ab842a6292468fa4f`;
- PI4.1 -> PI4.2 patch: `2d50a4701746e8bc559c08d912c669aadc36bbbfc038dc0c9855c557c13ac0c7`.

## Authority boundary

Nothing in PI4.2 approves production minimum-round changes, production answer authority for the research harness, Exact Flat replacement, causal/Hex authority changes, HypoKosh/FiberBundle governance changes, natural-language semantic-parser claims, or official external leaderboard claims.

**Next evidence action: execute the already-frozen exact-model bundle unchanged when authorized/no-cost capacity is available (PI4.2b). Do not redesign the reasoning algorithm or substitute the model first.**
