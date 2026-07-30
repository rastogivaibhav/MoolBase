# GA Progress: Release Candidate Bundle

Date: 2026-07-04

## What changed

- Added `scripts/run_release_candidate_bundle.ps1`
- Added `scripts/run_release_candidate_bundle.sh`
- Added `reports/RELEASE_CANDIDATE_BUNDLE.md`
- Wired the release-candidate bundle doc into the GA evidence collector

## What the bundle does

The bundle script runs three steps in sequence:

1. GA readiness smoke with package verification skipped inside the harness
2. release install package creation and package-consumer verification
3. evidence bundle collection with package artifacts attached and archived

This reduces the manual release-review path to one command while preserving the individual GA and packaging evidence underneath.

## Latest local smoke

Command:

```powershell
.\scripts\run_release_candidate_bundle.ps1
```

Result:

- `release_candidate_bundle=true`
- GA summary:
  `reports/ga-readiness/20260704-225943/GA_READINESS_SUMMARY.md`
- package:
  `graphenedb-install-package.zip`
- evidence bundle:
  `reports/ga-evidence/20260704-230226/`
- evidence archive:
  `reports/ga-evidence/20260704-230226.zip`

Validation:

- GA readiness summary status: `PASS`
- evidence manifest missing required artifacts: `0`
- evidence manifest missing optional artifacts: `0`
- package archive, `.sha256`, and `.manifest.json` all present

## Remaining gap

This is still a local Windows smoke bundle. Enterprise GA still requires the same flow on approved release hosts plus the larger soak, fuzz, target-filesystem, target-hardware, and signing gates tracked elsewhere.
