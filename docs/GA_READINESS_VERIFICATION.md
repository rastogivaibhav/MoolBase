# GA Readiness Verification

`scripts/run_ga_readiness.ps1` and `scripts/run_ga_readiness.sh` coordinate the current GA readiness gates into one preserved report directory.

After a GA readiness run, collect the reviewer-facing evidence bundle with `scripts/collect_ga_evidence.ps1` or `scripts/collect_ga_evidence.sh`. The collector copies the active docs, known reports, latest GA readiness run, optional package artifacts, git status, and an `EVIDENCE_MANIFEST.json` with SHA-256 hashes for copied files.

If `graphenedb-install-package.zip` exists at the repo root, the collector auto-includes the package archive, SHA-256 sidecar, and manifest without requiring `-PackagePath`/`PACKAGE_PATH`. The summary also prints the package-sidecar and manifest paths for quick review.

If the shell hosts are unavailable or unstable, `scripts/collect_ga_evidence.py` provides the same bundle shape and can archive the bundle directly with `--archive`.

To summarize current local evidence versus remaining GA gates, generate:

```powershell
.\scripts\write_ga_status_report.ps1
```

or:

```bash
scripts/write_ga_status_report.sh
```

The verifier runs:

- CMake configure
- build
- runnable CTest suite
- focused lattice/ACID/crash gates
- package install and external consumer smoke test
- recovery rehearsal
- Kosh adapter interchange gate
- vector-index recall benchmark
- extraction ingestion benchmark
- storage/retrieval benchmark

Each run writes logs under:

```text
reports/ga-readiness/<timestamp>/
```

The summary file is:

```text
reports/ga-readiness/<timestamp>/GA_READINESS_SUMMARY.md
```

Use `-ProfileLabel` / `PROFILE_LABEL` and `-ApprovedHost` / `APPROVED_HOST=1` when the run is on the approved release host that should count toward the remaining public-preview and enterprise-host gates.

For the heavier enterprise-facing validation campaign, use:

```powershell
.\scripts\run_enterprise_ga_campaign.ps1
```

or:

```bash
scripts/run_enterprise_ga_campaign.sh
```

That campaign preserves:

- full CTest output
- GA readiness harness output
- 100k stress output
- 1M storage output
- soak output
- optional real filesystem and disk-pressure gates
- optional coverage-fuzz output

under:

```text
reports/enterprise-ga/<timestamp>/
```

The enterprise campaign summary now records whether the run was actually release-like enough to count toward enterprise GA evidence. In particular, it calls out:

- `full_ctest_completed`
- `ga_readiness_completed`
- `fuzz_completed`
- `filesystem_gate_passed`
- `disk_pressure_gate_passed`
- `target_scale_dimensions_ready`
- `full_day_soak_profile_ready`
- `release_like_profile_ready`

`# Final Status` is only `PASS` when that release-like profile is actually satisfied. Local smokes and shortened runs should normally end as `PARTIAL`.

For the publishable developer-preview benchmark bundle, use:

```powershell
.\scripts\run_preview_hardware_profile.ps1
```

or:

```bash
scripts/run_preview_hardware_profile.sh
```

That profile preserves the benchmark logs and raw outputs for:

- vector-only vs Graphene comparison
- vector-index recall
- extraction ingest
- storage/retrieval
- host hardware/toolchain profile

under:

```text
reports/preview-hardware/<timestamp>/
```

Use `-ProfileLabel` / `PROFILE_LABEL` and `-IntendedHardware` / `INTENDED_HARDWARE=1` when the run is on the actual developer-preview machine class you intend to publish.

That preview profile also accepts explicit backend controls for the heavier benchmark legs:

- `-ExtractionVectorIndex` / `EXTRACTION_VECTOR_INDEX`
- `-StorageVectorIndex` / `STORAGE_VECTOR_INDEX`
- `-UseFaiss` / `GRAPHENEDB_USE_FAISS=1` when the preview host has FAISS provisioned

When those options are used, the preview summary and GA status report preserve both the requested and resolved vector index for the vector-recall, extraction-ingest, and storage/retrieval runs.

## Windows Smoke

```powershell
.\scripts\run_ga_readiness.ps1 `
  -VectorIndexRecallKind auto `
  -VectorIndexRecallMin 0.999 `
  -ExtractionDocs 10 `
  -ExtractionNodesPerDoc 20 `
  -ExtractionQueries 10 `
  -StorageNodes 2000 `
  -StorageQueries 20 `
  -Dim 32
```

## POSIX Smoke

```bash
VECTOR_INDEX_RECALL_KIND=auto \
VECTOR_INDEX_RECALL_MIN=0.999 \
EXTRACTION_DOCS=10 \
EXTRACTION_NODES_PER_DOC=20 \
EXTRACTION_QUERIES=10 \
STORAGE_NODES=2000 \
STORAGE_QUERIES=20 \
DIM=32 \
scripts/run_ga_readiness.sh
```

## Optional Performance Thresholds

The GA verifier can fail a run when benchmark metrics cross explicit thresholds.

Windows:

```powershell
.\scripts\run_ga_readiness.ps1 `
  -ExtractionThreshold "extract_nodes_per_sec>=1000","causal_lattice_p95_ms<=50" `
  -StorageThreshold "ingest_nodes_per_sec>=1000","causal_lattice_p95_ms<=50"
```

POSIX:

```bash
EXTRACTION_THRESHOLDS="extract_nodes_per_sec>=1000 causal_lattice_p95_ms<=50" \
STORAGE_THRESHOLDS="ingest_nodes_per_sec>=1000 causal_lattice_p95_ms<=50" \
scripts/run_ga_readiness.sh
```

Threshold rules are parsed by `scripts/check_benchmark_thresholds.py` and support `>=`, `<=`, `>`, `<`, and `==`.

The GA readiness harness also accepts explicit backend controls for the benchmark legs:

- `-ExtractionVectorIndex` / `EXTRACTION_VECTOR_INDEX`
- `-StorageVectorIndex` / `STORAGE_VECTOR_INDEX`
- `-UseFaiss` / `GRAPHENEDB_USE_FAISS=1` when the verifier host has FAISS provisioned

When those options are used, `GA_READINESS_SUMMARY.md` records the requested and resolved vector index for the vector-recall, extraction-ingest, and storage/retrieval runs.

## GA Profile

A GA release run should increase benchmark sizes on target hardware and preserve the generated report directory as release evidence.

Minimum suggested profile:

- `EXTRACTION_DOCS=1000`
- `EXTRACTION_NODES_PER_DOC=100`
- `EXTRACTION_QUERIES=500`
- `STORAGE_NODES=100000`
- `STORAGE_QUERIES=500`
- `DIM=384`

The rich 100k/1M stress harnesses now exercise realistic lattice/extraction-style workloads. What still remains for enterprise-scale performance claims is approved-host evidence at target dimensions: use at least `384` for the 100k profile and `768` for the 1M profile, then preserve those runs in the enterprise campaign output.

On locked-down Windows developer machines, local Application Control can still block some newly built executables. The `graphenedb_cli_extract_tests` gate now uses a Python wrapper on Windows so the full release-tree CTest suite can still pass locally. If another binary is blocked, use `-CtestExclude` or `-FocusedRegex` only for local smoke runs and preserve the failed report as policy evidence.

For local Windows validation where Clang/libFuzzer is unavailable, the enterprise campaign may be run with `-SkipFuzz` and treated as partial evidence only. A real enterprise GA run still requires preserved fuzz output from a host with the proper toolchain.

On Windows, an `llvm-mingw` `clang++` target such as `x86_64-w64-windows-gnu` is not enough by itself because it does not support the repo's `-fsanitize=fuzzer` build. In that case, run the fuzz gate on Linux/WSL or another Clang environment with libFuzzer support.

If local Application Control blocks the full CTest suite in a different environment, run the enterprise campaign with `-SkipFullCtest` to preserve a partial local bundle while keeping the approved-host full-suite requirement explicit.

If that same policy blocks the GA readiness harness in a fresh build directory, use `-SkipGaReadiness` for the local bundle and keep the approved-host GA harness run as a separate required artifact.

## Target-Scale Enterprise Preset

For the eventual approved-host enterprise evidence run, use the target-scale preset wrappers:

```powershell
.\scripts\run_target_scale_enterprise_profile.ps1
```

or:

```bash
scripts/run_target_scale_enterprise_profile.sh
```

Those presets default to:

- `100k`-class rich workload at `16667` six-node incident motifs and `DIM=384`
- `1M`-node rich workload at `DIM=768`
- approved-host attestation enabled
- a 24-hour soak
- a large fuzz run budget

They also accept explicit vector-index selections for the rich workload profiles:

- `-StressVectorIndex` / `STRESS_VECTOR_INDEX`
- `-OneMVectorIndex` / `ONE_M_VECTOR_INDEX`
- `-UseFaiss` / `GRAPHENEDB_USE_FAISS=1` when the host has FAISS provisioned and the campaign should compile against it
- `-SkipSoak` / `SKIP_SOAK=1` for local partial bundles when the soak executable is blocked or the host is unsuitable for a long run

When those options are used, the enterprise summary records both the requested and resolved vector index for the `100k` and `1m` profiles.

For local smoke validation, override the sizes and optionally skip long-duration gates:

```powershell
.\scripts\run_target_scale_enterprise_profile.ps1 `
  -ApprovedHost 0 `
  -SkipSoak `
  -SkipFullCtest `
  -SkipGaReadiness `
  -SkipFuzz `
  -SoakSeconds 2 `
  -StressIncidents 40 `
  -StressQueries 12 `
  -StressDim 16 `
  -OneMNodes 240 `
  -OneMQueries 12 `
  -OneMDim 16
```
