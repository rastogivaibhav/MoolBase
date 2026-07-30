# Release Candidate Bundle

GrapheneDB now has a one-command release-candidate bundle path that ties together:

- GA readiness smoke
- install package creation
- evidence bundle collection

By default it runs as `release-candidate-smoke` and preserves the requested/resolved backend selections for the vector-recall, extraction-ingest, and storage/retrieval legs.
The bundle also writes `reports/RELEASE_CANDIDATE_BUNDLE_META.json`, a machine-readable record of the invocation and resolved backend selections.
That metadata is validated before the wrapper exits, so schema drift is caught immediately.

## Commands

Windows:

```powershell
.\scripts\run_release_candidate_bundle.ps1
```

For an explicit internal-review profile:

```powershell
.\scripts\run_release_candidate_bundle.ps1 `
  -ProfileLabel release-candidate-smoke `
  -VectorIndexRecallKind auto `
  -ExtractionVectorIndex flat `
  -StorageVectorIndex kdtree
```

POSIX:

```bash
scripts/run_release_candidate_bundle.sh
```

For an explicit internal-review profile:

```bash
PROFILE_LABEL=release-candidate-smoke \
EXTRACTION_VECTOR_INDEX=flat \
STORAGE_VECTOR_INDEX=kdtree \
scripts/run_release_candidate_bundle.sh
```

## What it produces

- a preserved GA readiness run directory
- an install package archive
- package `.sha256` and `.manifest.json`
- `reports/RELEASE_CANDIDATE_BUNDLE_META.json`
- an archived GA evidence bundle that includes the latest GA run and package artifacts

## Intent

This is the fastest path for an internal release reviewer to inspect a candidate without manually stitching together multiple scripts. It is still a smoke or review bundle, not enterprise-GA proof by itself, and it deliberately stops short of approved-host soak, fuzz, and target-scale evidence.
