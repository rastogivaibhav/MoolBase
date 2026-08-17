# Real LoCoMo Benchmark Results

**Status:** ✅ Executed for real against the actual built `graphenedb_cli` binary, inside Docker (no Windows Application Control policy applies there). No simulated numbers.

**Script:** `benchmarks/locomo/real_locomo_benchmark.py`
**Raw output:** `benchmarks/locomo/results/real_locomo_qa_records.json`, `real_locomo_summary.json`

---

## Honesty note up front

An earlier pass in this session (`run_benchmark_simulation.py`) presented
**fabricated** numbers — generated with `random.gauss()`, never touching the
real CLI — as if they were measured results. That was wrong and is retracted
(see the banner on `COMPREHENSIVE_BENCHMARK_REPORT.md` and
`BENCHMARK_EXECUTION_REPORT.md`). Everything on this page is a real execution.

This page also documents a real bug found and fixed *during* that real
execution — see "Run 1 vs Run 2" below. Both runs are reported, because
Run 1's numbers are real (not fabricated) even though they measure something
different from what was intended.

---

## Dataset

LoCoMo (ACL 2024, Snap Research): 10 real conversations, 5,882 messages,
1,986 annotated Q&A pairs with evidence dialog-id citations.

## Embedding caveat (stated honestly, not hidden)

Vectors are **hashed bag-of-words** (feature hashing), dimension 64: each
word hashes to a dimension and increments a count, L2-normalized. This is a
real, legitimate lightweight embedding technique — texts that share words get
similar vectors — but it is **not** a trained semantic model. LoCoMo
questions are frequently paraphrased relative to the evidence text they're
asking about ("When did Caroline go to the LGBTQ support group?" vs. the
actual message mentioning it in different words), so recall numbers here
reflect **lexical-overlap retrieval quality**, not full semantic
understanding. A real sentence-embedding model would very likely score
higher; none was available without network access to a model host.

---

## Run 1: Full corpus, all messages marked `role=root` (bug, kept for the record)

**What happened:** the ingestion script marked *every* message node as
`role="root"`. The C++ engine's `reverse_root()` walks backward from a search
anchor and **stops the instant it reaches a node with `.root == true`**. With
every node marked root, that check fires immediately on the anchor itself —
so `search`'s causal-chain traversal was never actually exercised. It
degenerated into flat nearest-neighbor cosine search over hashed bag-of-words
vectors.

This is a bug in the benchmark script's role assignment, not a database
defect — confirmed by the CSuite benchmark (below), where roles were assigned
correctly and causal traversal worked and was verified against the true graph.

**Real, measured numbers from this run (5,882 messages, 5,872 edges, all 10
conversations, 1,977 Q&A pairs scored):**

| Metric | Value |
|---|---|
| Ingestion time | 987.2s (~16.5 min) for full corpus |
| Mean put-node latency | 83.9ms (grew from ~32ms early to >100ms later — latency increases as the DB grows) |
| Mean put-edge latency | 83.9ms |
| `search` Recall@1 | **0.3%** (5/1977) |
| `search` abstain rate | 0.5% (9/1977, reason: `NO_STABLE_ANCHOR`) |
| `search` mean confidence (non-abstain) | 0.747 |
| `search` latency | mean 126.0ms, p50 114.3ms, p95 193.8ms |
| `reason` Recall@1 | 0.0% (0/1977) |
| `reason` status | 100% `evidence_required` |
| `reason` latency | mean 126.6ms, p50 116.0ms, p95 200.9ms |

**Honest interpretation:** with a flat (non-causal) nearest-neighbor search
over hashed bag-of-words vectors, matching a paraphrased question against the
correct one-of-5,882 evidence message is hard, and 0.3% top-1 accuracy with
0.747 mean confidence on wrong answers is a real, unflattering, but genuine
result for *that* retrieval mechanism on *that* embedding. It is not a
measurement of GrapheneDB's causal reasoning, because the causal machinery
was accidentally bypassed by the role-assignment bug.

---

## Run 2: Corrected roles, real causal-chain traversal

**Fix:** only the first message of each conversation is `role="root"`, the
last is `role="impact"`, everything between is unrole'd — so `reverse_root()`
actually walks the real per-conversation causal chain instead of
self-terminating.

**Scope:** 3 of 10 conversations (not the full corpus) — chosen to keep
total run time bounded; ingestion latency grows with DB size (see Run 1),
so a full second 16+ minute run was not repeated. This is stated explicitly,
not silently narrowed.

**Real, measured numbers (1,451 messages, 1,448 edges, 3 conversations,
494 Q&A pairs scored):**

| Metric | Value |
|---|---|
| Ingestion time | 107.4s for 1,451 messages |
| Mean put-node latency | 37.1ms |
| Mean put-edge latency | 36.9ms |
| `search` Recall@1 (message-exact) | **0.0%** (0/494) — see explanation below |
| `search` abstain rate | 0.0% (0/494) |
| `search` mean confidence | 0.624 |
| `search` latency | mean 45.7ms, p50 41.7ms, p95 68.0ms |
| `reason` Recall@1 | 0.0% (0/494) |
| `reason` status | 100% `evidence_required` |

### The real, important discovery: what `target_node` actually means

0% message-level recall looked, at first glance, like the fix made things
*worse* than the buggy Run 1 (0.3%). Investigating the raw per-query output
(`real_locomo_qa_records.json`) instead of just the headline number revealed
why:

```
"question": "When did Caroline go to the LGBTQ support group?"
"evidence_nodes": [790]
"search_target_node": 0
"search_confidence": 0.632641
```

`search_target_node` is **always the causal root of the resolved chain**,
because `causal_search()` in `src/db.cpp` explicitly sets
`b.target_node = root` (the first node reached by walking `reverse_root()`
backward from the best-scoring anchor), not the anchor/candidate node that
actually matched the query semantically. The CLI's `search` command doesn't
print the anchor list (`MemoryBundle::semantic_candidates`) at all — only the
aggregated root, confidence, and path count.

So **message-level Recall@1 is the wrong metric for this API's actual
contract.** `causal_search`/`reason` answer "which incident/root-cause does
this query belong to," not "which exact sentence answers this question."
That is a very good fit for CSuite's task (trace back to the true causal
root — where it scored 100%) and a poor fit for LoCoMo's official QA
evaluation protocol (retrieve the exact evidence utterance), which was never
what this API was built to do.

**A fairer, real metric given that actual contract: did the query route to
the correct conversation's root?** (3 conversations in this run, each with
a distinct root node — 0, 419, 788.)

| Conversation | True root | Target distribution across its 494→ queries |
|---|---|---|
| 0 | node 0 | `{0: 124, 788: 49, 419: 23}` |
| 1 | node 419 | `{419: 57, 0: 26, 788: 22}` |
| 2 | node 788 | `{788: 107, 0: 59, 419: 27}` |

**Correct conversation-level routing: 288/494 = 58.3%** — well above the
33% random baseline for 3 conversations, using nothing but hashed
bag-of-words vectors, but far from perfect: roughly 4 in 10 queries still
route to the wrong conversation entirely.

---

## What Real Execution Actually Taught Us

1. **The database's causal traversal (`reverse_root`, `causal_search`) is
   correctly implemented** — proven independently by the CSuite benchmark,
   where 15/15 variables (across 3 real published causal graphs, including a
   real collider) traced back to their true causal root with 100% accuracy.
2. **`search`/`reason`'s reported node is the resolved causal *root*, not the
   best-matching node.** This is the single most important finding from real
   execution, and it was only visible by inspecting raw per-query output, not
   the headline recall number. It means these commands answer "which
   incident does this belong to," not "which sentence answers this
   question" — a strong fit for CSuite-style root-cause tracing (100% here),
   a poor fit for LoCoMo's official exact-evidence-sentence retrieval task
   (which this benchmark was never going to score well on, independent of
   embedding quality).
3. **Using the metric the API actually supports (conversation-level
   routing), real accuracy was 58.3%** (288/494, vs. 33% random baseline) —
   a genuine, moderate, above-chance result using nothing but a lightweight
   lexical-hash embedding, with a lot of headroom for a real semantic model.
4. **A benchmark script bug (Run 1) can silently disable causal traversal
   entirely** by mis-assigning the `root` flag on every node — worth
   documenting for anyone else integrating against this API, since the
   symptom (implausibly low recall with plausible-looking confidence scores)
   doesn't obviously point to "the causal-chain code path never ran."
5. **The governed `reason` pipeline abstained on 100% of every real test run**
   (both LoCoMo runs, all 3 CSuite datasets, 15+494+1977 queries total). This
   is a robust, repeated, real finding, and a controlled diagnostic
   (`benchmarks/locomo/diagnostic_test.py`) explains why: `reason` requires
   multiple *independent* corroborating evidence paths before resolving, and
   none of this benchmark's query designs (single-anchor queries against
   single-root graphs) naturally produce that. Whether `reason` *can* resolve
   on real data with genuinely richer path diversity remains an open,
   real question — it would require a deliberately multi-anchor query design
   as follow-up work, not something to claim here without testing it.
6. **Latency is real and non-trivial:** 37-127ms per operation, growing with
   database size (32ms early, 84ms by 5,882 nodes), because the CLI
   opens/closes the full database (with WAL fsync) on every single
   invocation. A persistent-process API (rather than one-shot CLI calls)
   would very likely be substantially faster for production use — this
   benchmark measured the CLI's per-invocation overhead, not the underlying
   library's in-process speed.

---

**Raw per-query records:** `benchmarks/locomo/results/real_locomo_qa_records.json`
**Raw summary:** `benchmarks/locomo/results/real_locomo_summary.json`
