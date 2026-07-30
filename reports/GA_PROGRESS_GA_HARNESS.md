# GA Progress: Unified Readiness Harness

Date: 2026-07-04

## What changed

- `scripts/run_ga_readiness.ps1` now includes:
  - recovery rehearsal
  - vector-index recall benchmark
- `scripts/run_ga_readiness.sh` now includes:
  - recovery rehearsal
  - vector-index recall benchmark
- The GA readiness docs and CI release automation notes now describe those gates.

## Why it matters

The readiness harness now exercises more of the actual release story in one preserved run. A reviewer can inspect one `reports/ga-readiness/<timestamp>/` directory and see not only build, CTest, package, extraction, and storage evidence, but also:

- backup/restore rehearsal output
- vector-index recall output versus exact flat search

## Latest local smoke

Command:

```powershell
.\scripts\run_ga_readiness.ps1 -SkipPackage -VectorIndexRecallNodes 200 -VectorIndexRecallQueries 10 -VectorIndexRecallDim 16 -VectorIndexRecallK 5 -VectorIndexRecallKind auto -ExtractionDocs 2 -ExtractionNodesPerDoc 4 -ExtractionQueries 2 -StorageNodes 200 -StorageQueries 4 -Dim 16
```

Result:

- `reports/ga-readiness/20260704-225701/GA_READINESS_SUMMARY.md`
- status: PASS
- recovery rehearsal: PASS
- vector index recall benchmark: PASS
- extraction ingest benchmark: PASS
- storage retrieval benchmark: PASS

## Remaining gap

This is still a local Windows smoke run. Full GA proof still requires the default harness on an approved host without policy exclusions, plus the larger scale, soak, fuzz, and target-filesystem campaigns already tracked in the GA scorecard.
