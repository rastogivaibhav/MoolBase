# Cross-dataset FiberBundle and Lyapunov protocol

## Purpose

Test whether the FiberBundle v2 and Lyapunov critic remediation generalises beyond the 2Wiki validation sample.

The suite uses real gold support annotations from three structurally different benchmark families:

- bAbI for controlled one-, two- and three-step logical, temporal, negative and positional reasoning;
- HotpotQA distractor for multi-document supporting facts mixed with irrelevant documents;
- FEVER for supported, refuted and not-enough-information claim states.

## Scope boundary

This is a component diagnostic. It does not test retrieval, language understanding, answer generation, exact match, supporting-fact F1 or truth probability.

The preparer writes only benchmark identifiers, labels and evidence-shape counts. The C++ executable constructs controlled FiberBundles through the repository's actual `FiberBundleBuilder` and assesses them with the actual `LyapunovCritic`.

## Frozen sample

Default seed: `20260729`.

Default sample size:

- up to 1,000 bAbI questions across tasks 2, 3, 9, 10, 14, 15, 17 and 19;
- 1,000 HotpotQA distractor validation examples;
- approximately 1,000 balanced FEVER development examples across SUPPORTS, REFUTES and NOT ENOUGH INFO.

The first successful run records the manifest and becomes the locked evaluation sample. Later remediation must use a separate development sample and rerun the locked sample unchanged.

## Controlled mutations

Every evidence-bearing example is assessed under:

1. complete gold evidence shape;
2. one missing critical fact;
3. a different graph route using the same source and evidence family;
4. an irrelevant low-relevance distractor;
5. a material contradictory path;
6. an independently sourced supporting path;
7. a missing-to-gold repair trajectory.

FEVER labels add these state tests:

- SUPPORTS must not be marked contradictory;
- REFUTES must block resolution;
- NOT ENOUGH INFO must abstain.

## Initial targets

| Diagnostic | Target |
|---|---:|
| Missing critical fact raises energy | >=99% |
| Material contradiction raises energy | >=99% |
| Same-source duplicate not rewarded | 100% |
| Irrelevant distractor not rewarded | 100% |
| Independent support improves or preserves energy | >=99% |
| Duplicate independence inflation | 0% |
| Material contradiction blocks resolution | 100% |
| Repair trajectory monotonic | >=95% |
| FEVER REFUTES blocked | 100% |
| FEVER NOT ENOUGH INFO abstains | 100% |

The first run remains diagnostic and always uploads evidence. A miss must be reported rather than hidden by changing weights on the evaluation sample.

## Execution branch

The benchmark is developed and executed on `benchmark/cross-dataset-epistemic-suite`, based on the remediated FiberBundle v2/Lyapunov head. Results must not be merged into the remediation PR as positive evidence until the workflow has completed and its manifest and artifact are preserved.
