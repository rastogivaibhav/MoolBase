# Portable cross-dataset validation

This pack runs the cross-dataset epistemic diagnostic without GitHub-hosted Actions. It compiles the repository's actual `FiberBundleBuilder` and `LyapunovCritic`, enforces the committed offline regression, optionally downloads the frozen public sample, and packages immutable evidence.

## Claim boundary

The suite evaluates evidence topology, source independence, contradiction handling, missing evidence, temporal validity, noise quarantine and governed stability. It does **not** measure question-answer exact match, retrieval recall, supporting-fact F1, generated reasoning quality or semantic truth probability.

## Linux or macOS

From the repository root:

```bash
chmod +x scripts/run_cross_dataset_local.sh
scripts/run_cross_dataset_local.sh offline
```

The public-data run is:

```bash
scripts/run_cross_dataset_local.sh public
```

The defaults are frozen at:

- 200 records per selected bAbI task: 1,400 total;
- 500 HotpotQA distractor-validation records;
- 200 FEVER claims per label: 600 total;
- seed `20260729`.

Override counts only for smoke testing through `BABI_PER_TASK`, `HOTPOT`, `FEVER_PER_LABEL`; do not describe a reduced run as the frozen 2,500-record result.

## Windows PowerShell

Open a Developer PowerShell for Visual Studio, or install MinGW-w64, then run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\run_cross_dataset_windows.ps1 -Mode offline
```

For public data:

```powershell
.\scripts\run_cross_dataset_windows.ps1 -Mode public
```

The runner detects `g++` first and then `cl.exe`.

## Docker

Offline regression:

```bash
docker build -f Dockerfile.cross-dataset -t graphenedb-cross-dataset .
docker run --rm \
  -v "$PWD/reports/cross_dataset/portable:/workspace/reports/cross_dataset/portable" \
  graphenedb-cross-dataset offline
```

Frozen public run:

```bash
docker run --rm \
  -v "$PWD/reports/cross_dataset/portable:/workspace/reports/cross_dataset/portable" \
  graphenedb-cross-dataset public
```

## Evidence produced

Each successful run writes:

```text
reports/cross_dataset/portable/
├── source_isolated_results.csv
├── source_isolated_summary.json
├── source_isolated_summary.md
├── public_results.csv                 # public/all mode
├── public_summary.json                # public/all mode
├── public_summary.md                  # public/all mode
├── evidence/
│   ├── environment.json
│   ├── source_commit.txt
│   ├── checksums.sha256
│   └── copies of all result and manifest files
└── cross-dataset-<mode>-<commit>.tar.gz or .zip
```

`environment.json` records the source commit, dirty-tree state, OS, Python and compiler versions, dataset counts, input sizes and SHA-256 hashes. `checksums.sha256` signs the evidence directory.

## Reproducibility gates

A result is acceptable only when:

1. the committed source-isolated fixture passes before network access;
2. compilation uses `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang, or `/W4 /WX` on MSVC;
3. the public manifest preserves seed, record counts and downloaded-payload hashes;
4. the summary script exits successfully under `--enforce`;
5. the evidence archive includes the exact source commit and a clean/dirty working-tree declaration;
6. no thresholds are changed after seeing the frozen evaluation sample.

## Interpretation

A one-route gold chain can remain outside Lyapunov equilibrium because it lacks independent corroboration. That is expected. Success means duplication and irrelevant retrieval cannot improve the assessment, material contradiction blocks resolution, insufficient evidence triggers repair or abstention, and independent relevant sources improve or preserve support.

The Lyapunov critic remains a dynamics and admissibility controller, not a truth oracle. Semantic correctness must be provided by retrieval, a domain verifier, benchmark labels or human review.
