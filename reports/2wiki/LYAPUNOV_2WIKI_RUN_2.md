# Lyapunov Critic on 2WikiMultiHopQA — Remediation Run 2

Date: 28 July 2026  
Branch: `fix/fiberbundle-v2-lyapunov`  
Tested source head: `65e6312ee70c62495987a7baacbca2dc46d42d66`  
Workflow run: `30409096442`  
Artifact: `8707705268`  
Artifact digest: `sha256:7c985bcc0484fe6aeb306a189c2175abe2207378b47659dd1cc9074a719ded43`

## Scope

This run evaluates the corrected FiberBundle v2 and Lyapunov critic on the unchanged deterministic 1,000-example 2WikiMultiHopQA validation sample used for Run 1.

The implementation under test separates graph-route identity from evidence-family independence, quarantines irrelevant paths, treats material opposition as an admissibility blocker, and keeps semantic verification separate from reasoning dynamics.

## Dataset

- Dataset: `framolfese/2WikiMultihopQA` validation split.
- Sample: 1,000 examples.
- Seed: `20260728`.
- Question types: 410 compositional, 248 comparison, 223 bridge-comparison and 119 inference.
- Temporal coordinate: not tested because the dataset does not provide dependable event-validity timestamps.

## Locked validation results

| Diagnostic | Run 1 | Run 2 | Required target | Verdict |
|---|---:|---:|---:|---|
| Missing-hop energy above gold | 100.0% | 100.0% | >=90% | Pass |
| Contradiction energy above gold | 0.0% | 100.0% | >=90% | Pass |
| Same-source duplicate not rewarded | 0.0% | 100.0% | >=98% | Pass |
| Irrelevant distractor not rewarded | 0.0% | 100.0% | >=95% | Pass |
| Missing → gold trajectory monotonic | 100.0% | 100.0% | >=90% | Pass |

The pre-registered diagnostic gate passed.

## Mean Lyapunov energy

| Condition | Run 1 | Run 2 | Behaviour after remediation |
|---|---:|---:|---|
| Gold evidence chain | 0.133082 | 0.090251 | Baseline admissible structure; still outside equilibrium because one canonical chain is not independent corroboration |
| Missing hop | 0.321205 | 0.799893 | Strongly penalised for incompleteness and truncation |
| Contradictory alternative | 0.028737 | 0.850000 | Material opposition now imposes a hard energy barrier |
| Same-source duplicate | 0.000000 | 0.090251 | Exactly equal to gold; route duplication creates no evidence benefit |
| Irrelevant distractor | 0.000000 | 0.315789 | Noise increases energy and cannot reduce pattern lock |
| Structurally complete wrong path | 0.133082 | 0.090251 | Remains equal to gold for dynamics, as expected; semantic verification is a separate gate |

## Corrected behavioural invariants

- Same-source route variants increased independent support in **0.0%** of examples.
- Same-source route variants reduced pattern lock in **0.0%** of examples.
- Irrelevant distractors reduced pattern lock in **0.0%** of examples.
- Contradictory, duplicate, distractor and missing-hop bundles were classified stable in **0.0%** of examples.
- All four 2Wiki question types achieved 100% missing-hop, contradiction, duplicate-safety and distractor-safety rates.

## Interpretation

The critic now preserves the intended monotonic ordering:

```text
gold <= correlated duplicate < irrelevant distractor < missing evidence / material contradiction
```

A duplicate is deliberately equal to the gold baseline rather than better than it. An irrelevant path is retained for audit but adds a retrieval-noise cost. A material contradiction cannot be cancelled by path diversity.

The wrong-complete condition remains indistinguishable from gold to the dynamics critic. This is intentional and confirms the corrected architectural boundary: Lyapunov energy evaluates evidence structure and reasoning dynamics, while semantic correctness is handled by the separate verification and governed-projection layer.

## Remaining claim boundaries

This successful run does not establish:

- answer exact match or retrieval recall;
- semantic truth verification without a verifier;
- temporal reasoning quality;
- formally proved Lyapunov stability;
- production readiness.

It demonstrates that the specific adversarial ordering defects exposed by Run 1 have been corrected on the locked 2Wiki validation sample.
