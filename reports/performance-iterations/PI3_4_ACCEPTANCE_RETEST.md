# PI3.4 Acceptance Retest — 2026-08-19

## Verdict

**PASS — PI3.4 is accepted as a controlled experimental integration candidate.**

This retest executed the canonical `graphenedb_performance_iteration_3_4_source.zip` snapshot after verifying its recorded SHA-256.

Canonical source SHA-256:

`f9837126bb92ded1a39ead678267b0eab5cc6cfcde41a7567abd075ce03b40e9`

## Live acceptance results

- permanent `run_performance_iteration_3_4_gate.sh`: **PASS**
- structured semantic recall mean/worst: **1.000 / 1.000**
- unseen-query semantic recall mean/worst: **1.000 / 1.000**
- incomplete-causal root survival through causal/provenance only: **0.00**
- incomplete-causal root survival with Dense Hex + causal/provenance: **1.00**
- full required-object survival: **1.00**
- vector-only scale recall mean/worst: **1.000 / 1.000**
- complete product regression: **66/66 PASS, 0 failures**
- GCC ThreadSanitizer promotion-readiness test: **PASS**, `halt_on_error=1`, no race report
- Clang 17 ASAN + UBSAN promotion-readiness test: **PASS**, no sanitizer finding
- architecture conservation: **PASS**
- deterministic shadow rebuild after GrapheneDB reopen: **PASS**

## Accepted boundary

The int16 semantic shadow may proceed only to a controlled, disabled-by-default integration layer. The authoritative vector remains `Node::vector`; shadow candidates must be exact-rescored before admission.

PI3.4 does **not** approve:

- `VectorIndexKind::Auto` selection;
- server or CLI default selection;
- semantic answer authority;
- a new durable shadow format;
- claims about 1M x 768D;
- asymptotically better than O(N x D) screening.

## Remote materialisation note

The validated canonical source is ahead of the current remote repository baseline. See `updates/performance-iteration-3.4/REMOTE_SOURCE_STATUS.md` for the exact master-to-PI3.4 reconciliation counts, hashes, and why this PR remains draft until the full canonical tree is materialised.
