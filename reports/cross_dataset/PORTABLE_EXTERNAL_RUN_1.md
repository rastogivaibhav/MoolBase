# Portable external cross-dataset validation — Run 1

Branch: `benchmark/cross-dataset-epistemic-suite`

Portable runner head at execution: `80dba15010492cd2f5d521de6f1a306a93a136df`

## Purpose

Validate the cross-dataset epistemic suite outside GitHub-hosted Actions, while preserving a strict boundary between an actual-source result, a source-equivalent mirror, and the still-pending downloaded public-data run.

## Repository-side deliverables

The branch now contains:

- `scripts/run_cross_dataset_local.sh`;
- `scripts/run_cross_dataset_windows.ps1`;
- `scripts/capture_cross_dataset_environment.py`;
- `Dockerfile.cross-dataset`;
- `docs/benchmarks/PORTABLE_CROSS_DATASET_VALIDATION.md`.

These runners compile the actual C++ `FiberBundleBuilder` and `LyapunovCritic`, enforce the committed 46-record fixture first, optionally run the frozen public sample, and package source commit, environment, raw results, manifests and SHA-256 checksums.

## Execution performed in the network-isolated sandbox

The private repository could not be authenticated and public benchmark payloads could not be downloaded from the sandbox. An independent executable mirror of the current structural equations was therefore used only after calibration against the committed actual-C++ fixture result.

### Actual-C++ fixture equivalence

- Fixture records: **46**;
- bAbI support records: **40**;
- HotpotQA support records: **3**;
- FEVER records: **3** (`SUPPORTS`, `REFUTES`, `NOT ENOUGH INFO`);
- absolute energy comparison tolerance: **1e-12**;
- aggregate equivalence to committed actual-C++ summary: **PASS**;
- all 13 enforced diagnostic gates: **PASS**.

Observed mean support-case energies:

| Condition | Energy |
|---|---:|
| Gold | 0.09025083144280861 |
| Missing evidence | 1.0 |
| Same-source duplicate | 0.09025083144280861 |
| Irrelevant distractor | 0.3157894736842105 |
| Material contradiction | 0.8833333333333333 |
| Independent corroboration | 0.0 |

### Frozen-shape 2,500-record sweep

A generated record-ID sweep used the frozen distribution:

- 1,400 bAbI support shapes;
- 500 HotpotQA support shapes;
- 200 FEVER support shapes;
- 200 FEVER refutation shapes;
- 200 FEVER insufficient-evidence shapes.

All frozen gates passed at **100%**. The sweep verifies deterministic scaling and structural invariants; it is **not** the downloaded public-data benchmark.

## Claim boundary

The only result tied directly to actual C++ execution remains the committed 46-record run. The mirror matched that aggregate result but does not replace actual-source compilation. The 2,500 shape sweep uses generated identifiers and must not be called the public benchmark.

The real public run remains:

```bash
scripts/run_cross_dataset_local.sh public
```

or its Windows/Docker equivalent on a networked clone. No thresholds were modified after observing the frozen fixture.

The benchmark measures evidence structure and governed stability. It does not measure answer exact match, retrieval recall, supporting-fact F1, generated semantic correctness or truth probability.
