# Real Microsoft CSuite Benchmark Results

**Status:** ✅ Executed for real against the actual built `graphenedb_cli` binary, inside Docker (no Windows Application Control policy applies there). No simulated numbers.

**Script:** `benchmarks/csuite/real_csuite_benchmark.py`
**Raw output:** `benchmarks/csuite/results/real_csuite_results.json`

---

## Scope (stated honestly)

CSuite ships the **true causal graph** for each dataset (`adj_matrix.csv`) plus
observational/interventional data rows for statistical causal-inference
benchmarking (ATE/CATE estimation).

GrapheneDB's CLI does **not** perform statistical regression or causal
discovery from raw data — it is a structured causal-memory store with a
governed reasoning layer. So this benchmark tests exactly that, honestly:

- Ingest the **true graph** (one node per variable, edges per `adj_matrix.csv`)
- Query with each variable's own real description
- Check: does `search` (causal_search) trace back to the correct true root?
- Check: does `reason` (the governed HypoKosh/Dialectic/Lyapunov pipeline)
  resolve, especially on **collider variables** that have 2+ real independent
  causal parents?

**Not tested / not claimed:** causal discovery from raw observational data,
ATE/CATE numeric estimation. GrapheneDB's CLI does not compute either, and no
numbers for them are reported here (earlier session output that claimed
"ATE RMSE" and "causal discovery accuracy" figures were simulated and have
been retracted — see `COMPREHENSIVE_BENCHMARK_REPORT.md` superseded notice).

---

## Real Datasets Used

Downloaded directly from `github.com/microsoft/csuite` releases:

| Dataset | Variables | True edges | Colliders (in_degree > 1) |
|---|---|---|---|
| `csuite_lingauss` | 2 | 1 | 0 |
| `csuite_nonlin_simpson` | 4 | 4 | 1 (node x2, Simpson's-paradox confounder) |
| `csuite_large_backdoor` | 9 | 9 | 1 (node x8, backdoor collider with 2 independent parent chains: `0→1→3→5→7→8` and `0→2→4→6→8`) |

---

## Real Results

### `search` (causal_search) — structural retrieval

| Dataset | Correct-root retrieval |
|---|---|
| csuite_lingauss | 2/2 (100%) |
| csuite_nonlin_simpson | 4/4 (100%) |
| csuite_large_backdoor | 9/9 (100%) |
| **Total** | **15/15 (100%)** |

For every variable in every dataset, `search` correctly traced the causal
chain back to the true root variable defined in the published `adj_matrix.csv`
— including the collider node `x8`, which merged **9 independent paths**
(`search_paths=9`) into its bundle, and still resolved with confidence 1.0.

### `reason` (governed HypoKosh/Dialectic/Lyapunov pipeline)

| Dataset | Resolved (non-abstain) |
|---|---|
| csuite_lingauss | 0/2 |
| csuite_nonlin_simpson | 0/4 |
| csuite_large_backdoor | 0/9 (including the collider, 0/1) |
| **Total** | **0/15** |

`reason` abstained with `status=evidence_required` on every single query,
even on the collider variable where `search` found 9 converging paths.

---

## Why `reason` Abstained — Real Investigation, Not Guesswork

We didn't stop at "it abstained" — we checked why, with a controlled
diagnostic (`benchmarks/locomo/diagnostic_test.py`):

- `search` resolves confidently (confidence 0.72–1.0) on both exact-text and
  paraphrased queries.
- `reason` abstains **even on an exact-text match to a single stored node**,
  reporting uncertainty reasons including *"no independent support candidate
  is available"*, *"no support-eligible evidence groups remain"*, and *"the
  selected target has fewer than two independent evidence families."*

This means `reason`'s bar is not just "does search find something" — it
requires multiple **evidentially independent** corroborating paths, and our
query design (querying with a variable's own description, which makes that
variable the single dominant search anchor) only ever activates one anchor's
`reverse_root` walk, or several near-duplicate paths sharing the same generic
vocabulary rather than truly independent evidence sources. The `search_paths=9`
figure includes many paths built from shared boilerplate tokens ("large
backdoor", "type continuous") across all 9 variables, not necessarily 9
evidentially distinct corroborations. That distinction between raw path count
and evidentiary independence is exactly what `reason`'s stability critic /
opposition layer is designed to catch, per `no_silent_promotion` in the
codebase.

**Honest conclusion:** this benchmark's query design (single-node embeddings)
is not sufficient to exercise `reason`'s multi-path resolution path. Testing
that properly requires deliberately constructed multi-anchor queries (e.g.
querying with a vector that legitimately overlaps two independently-described
parent variables at once), which is future work, not something we
retroactively adjusted results to claim here.

---

## What This Real Benchmark Actually Demonstrates

1. **GrapheneDB correctly stores and retrieves arbitrary causal DAGs**,
   including genuine branching/converging structures (collider nodes),
   verified against Microsoft's published ground-truth graphs — 100% correct
   on 15/15 variables across 3 datasets.
2. **The governed `reason` pipeline is conservative by design** — it did not
   confidently resolve on any of these 15 queries, consistent with its
   `no_silent_promotion` philosophy, and consistent with what the same
   diagnostic showed on LoCoMo conversational data.
3. **GrapheneDB is not, and was not tested as, a causal-discovery or
   statistical treatment-effect engine.** Any prior claim otherwise (ATE
   RMSE, CATE RMSE, "89.7% causal discovery accuracy") was fabricated in an
   earlier, simulated pass and is retracted.

---

**Raw per-variable records:** `benchmarks/csuite/results/real_csuite_results.json`
