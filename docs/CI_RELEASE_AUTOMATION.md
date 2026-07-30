# CI And Release Automation

The GitHub Actions workflow runs:

- release build/test on Linux, macOS, and Windows
- package install consumer smoke on all platforms
- GA readiness smoke on Linux
- release-candidate bundle smoke on Linux
- sanitizer smoke on Linux
- fuzz smoke on Linux

The GA readiness smoke uploads `reports/ga-readiness/` plus `reports/GA_STATUS_REPORT.md` as a workflow artifact so the run has preserved logs and a current status summary. It now also covers the recovery rehearsal and vector-index recall benchmark, so the artifact reflects more than the older acid/package/storage subset.

The release-candidate bundle smoke uploads a labeled `release-candidate-smoke` evidence set unless overridden, and it preserves:

- the package archive
- the package `.sha256`
- the package `.manifest.json`
- `reports/ga-readiness/`
- `reports/ga-evidence/`
- `reports/GA_STATUS_REPORT.md`

The bundle also carries through the requested and resolved vector-index selections for the vector-recall, extraction-ingest, and storage/retrieval benchmark legs, so the archive is easier to audit against the actual backend choices that ran.
It also writes `reports/RELEASE_CANDIDATE_BUNDLE_META.json`, which records the requested profile label plus resolved benchmark backend selections for the archive itself.
That metadata is validated by `scripts/validate_release_candidate_bundle_meta.py` before the bundle exits, so schema drift fails fast.

Example internal-review invocation:

Windows:

```powershell
.\scripts\run_release_candidate_bundle.ps1 `
  -ProfileLabel release-candidate-smoke `
  -VectorIndexRecallKind auto `
  -ExtractionVectorIndex flat `
  -StorageVectorIndex kdtree
```

POSIX:

```bash
PROFILE_LABEL=release-candidate-smoke \
EXTRACTION_VECTOR_INDEX=flat \
STORAGE_VECTOR_INDEX=kdtree \
scripts/run_release_candidate_bundle.sh
```

That bundle stays intentionally below enterprise GA proof. It is a reviewable smoke package for the packaged release path, not a substitute for approved-host soak, fuzz, or target-scale campaigns.

## Local Release Package

POSIX:

```bash
scripts/package_release_install.sh graphenedb-install-package.zip
```

Windows:

```powershell
.\scripts\package_release_install.ps1 -Out graphenedb-install-package.zip
```

The package scripts:

1. Configure and build Release.
2. Install to a local prefix.
3. Run the installed-package consumer verification.
4. Archive the install prefix.
5. Write a release manifest and SHA-256 sidecar.

The resulting archive is the preferred distribution artifact because it contains the installed layout consumers actually use:

- `bin/graphenedb_cli`
- `include/graphene/*`
- `lib/*`
- `lib/cmake/GrapheneDB/*`
- `share/doc/GrapheneDB/*`

## Release Evidence

Before tagging a release, preserve:

- the CI run URL
- the GA readiness artifact
- the package archive
- the package `.sha256` sidecar
- the package `.manifest.json` file with per-file hashes
- benchmark logs for the target hardware profile
- any platform-specific exclusions or policy constraints

For a fast internal review bundle, use:

Windows:

```powershell
.\scripts\run_release_candidate_bundle.ps1
```

POSIX:

```bash
scripts/run_release_candidate_bundle.sh
```
