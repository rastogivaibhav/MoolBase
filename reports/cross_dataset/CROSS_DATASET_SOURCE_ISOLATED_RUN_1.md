# Cross-dataset epistemic suite — source-isolated run 1

Date: 29 July 2026  
Benchmark branch: `benchmark/cross-dataset-epistemic-suite`  
Remediation base: `77a4b3c39b6158a443f601b9b8a69e2a387c761e`

## Scope

This run evaluates the materialised `FiberBundleBuilder` and `LyapunovCritic`
against 46 benchmark-derived records from three benchmark families:

- 40 bAbI records: 20 two-supporting-fact questions and 20 temporal-reasoning questions;
- 3 HotpotQA records: comparison and bridge evidence shapes with distractor documents;
- 3 official FEVER examples: one `SUPPORTS`, one `REFUTES`, and one `NOT ENOUGH INFO`.

The normalized fixture contains IDs and evidence-shape metadata only. It does not
copy question, answer, claim, or context text.

## Execution

The harness was compiled as C++20 with:

```text
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror
```

It linked the current `src/fiber_bundle.cpp` and `src/stability_critic.cpp`
directly, then enforced the frozen diagnostic targets.

## Results

| Diagnostic | Result | Target |
|---|---:|---:|
| Missing critical evidence raises energy | 100.0% | >=99% |
| Material contradiction raises energy | 100.0% | >=99% |
| Same-source duplicate is not rewarded | 100.0% | 100% |
| Irrelevant distractor is not rewarded | 100.0% | 100% |
| Duplicate does not inflate independence | 100.0% | 100% |
| Independent corroboration is detected | 100.0% | >=99% |
| Independent corroboration does not reduce score | 100.0% | >=99% |
| Material contradiction blocks resolution | 100.0% | 100% |
| FEVER refutation blocks resolution | 100.0% | 100% |
| FEVER insufficient evidence is not stable | 100.0% | 100% |
| FEVER insufficient evidence requests repair/abstention | 100.0% | 100% |
| Temporal mismatch raises energy | 100.0% | >=99% |
| Temporal mismatch is not stable | 100.0% | 100% |

Overall frozen gate: **PASS**.

## Mean support-case energy

| Condition | Mean Lyapunov energy |
|---|---:|
| Gold evidence structure | 0.090251 |
| Missing evidence | 1.000000 |
| Same-source duplicate | 0.090251 |
| Irrelevant distractor | 0.315789 |
| Material contradiction | 0.883333 |
| Independent corroboration | 0.000000 |

The duplicate condition exactly preserved gold energy and independent-support
count. Independent corroboration increased the support count from one to two,
raised the aggregate score from `0.842344` to `0.963201`, and reached zero
structural energy.

## Important finding

The gold energy was identical across bAbI, HotpotQA, and FEVER support records
when their verified evidence structures reduced to one complete support route.
This confirms that the critic is deliberately content-agnostic: it evaluates
verifier-provided relevance, validity, lineage, contradiction, temporal status,
and evidence topology. It does not itself read benchmark text or determine
answer correctness.

Therefore this result validates the corrected epistemic-control invariants, but
it is **not** an end-to-end reasoning or semantic generalization score.

## Remaining full-run blocker

The committed public-data runner targets 2,500 records: 1,400 bAbI, 500
HotpotQA, and 600 balanced FEVER claims. GitHub Actions accepted the workflow,
but both the original attempt and one explicit retry terminated before checkout
and exposed no workflow steps or logs. The local container also had no external
DNS, so the full public files could not be downloaded there.

The 2,500-record run remains pending. This 46-record source-isolated result must
not be presented as a substitute for it.
