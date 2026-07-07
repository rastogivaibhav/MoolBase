# Claude Repo Context

This repository is GrapheneDB v0.5.0 RC5: an experimental embedded C++20 causal/lattice-memory database for AI memory, coding assistants, incident memory, research-pack ingestion, and team-brain workflows.

GrapheneDB is not a SQL database, distributed service, Qdrant replacement, Neo4j replacement, or material-science simulator. Its intended product boundary is an embedded library plus CLI. The central promise is durable memory nodes, embeddings, metadata, causal/versioned edges, and explainable retrieval bundles.

## Where To Look First

- `README.md` - product scope, status, quick start, scripts, and limitations.
- `docs/ARCHITECTURE.md` - storage, WAL, snapshots, search, and CLI structure.
- `docs/GRAPHENE_LATTICE_MODEL.md` - what the graphene-inspired lattice means and where the analogy stops.
- `docs/LATTICE_RETRIEVAL.md` - lattice-aware retrieval behavior.
- `docs/EXTRACTION_INGESTION.md` - source-scoped extraction ingestion contract.
- `docs/STORAGE_FORMAT.md` - durable component format notes.
- `docs/NEXT_GA_EXECUTION_PLAN.md` - release path and what remains for enterprise GA.
- `reports/GA_STATUS_REPORT.md` - current evidence snapshot.

## Code Map

- `include/graphene/` - public C++ headers and C ABI header.
- `src/` - database implementation, C ABI, platform layer, and lattice placement.
- `tools/graphenedb_cli.cpp` - operator CLI.
- `bench/` - benchmark programs used by GA/readiness scripts.
- `tests/` - unit, CLI, packaging, fault-injection, lattice, extraction, and C ABI tests.
- `scripts/` - repeatable build, package, evidence, benchmark, and release-candidate workflows.
- `reports/` - intentionally preserved release evidence and status reports.

## Current Release Posture

The current branch has a public developer-preview candidate with local evidence passing and `public_developer_preview: 0` pending items in `reports/GA_STATUS_REPORT.md`. It is still not enterprise GA. Enterprise GA remains blocked on long soak/fuzz, approved-host target-scale runs, real filesystem failure evidence on target hosts, release governance, and final vector backend decisions.

## Immediate Next Step For Handoff

The prepared branch is `codex/rc5-developer-preview`, pushed to `origin`.

The next agent should:

1. Open a GitHub pull request from `codex/rc5-developer-preview` into `master`.
2. Describe it as the RC5 public developer-preview candidate, explicitly not enterprise GA.
3. Cite the preserved evidence:
   - `reports/GA_STATUS_REPORT.md`
   - `reports/preview-hardware/20260707-141656/PREVIEW_HARDWARE_SUMMARY.md`
   - `reports/ga-readiness/20260707-142146/GA_READINESS_SUMMARY.md`
   - `reports/ga-evidence/20260707-142233.zip`
4. Monitor CI and fix only CI, portability, packaging, or reviewer issues needed to land the PR.
5. Merge to `master` when CI/review are acceptable.
6. Tag or draft the developer-preview release as `GrapheneDB v0.5.0-rc5 - embedded causal/lattice-memory DB for controlled pilots`.

Do not expand scope into enterprise GA in this PR. The enterprise-GA backlog remains the long-running approved-host soak/fuzz/target-scale/filesystem/signing work in `docs/NEXT_GA_EXECUTION_PLAN.md`.

## Engineering Constraints

- Preserve the embedded-library boundary unless the product plan changes.
- Keep durable storage changes explicit, documented, and tested.
- Do not claim carbon physics; lattice coordinates are memory topology and retrieval hints.
- Treat `put_batch()`, `put_extraction()`, CLI import/extract/inspect/validate/backup/compact/neighbors, and the minimal C ABI as public-facing surfaces.
- Prefer small, deterministic tests. Use preserved report artifacts for release claims.
- Do not commit build directories or local scratch outputs. Commit `reports/` evidence only when it is intentional release evidence.

## Useful Commands

Windows:

```powershell
.\scripts\run_release_candidate_bundle.ps1
.\scripts\run_preview_hardware_profile.ps1 -ProfileLabel developer-preview -IntendedHardware
.\scripts\verify_package_install.ps1
```

POSIX:

```bash
scripts/run_release_candidate_bundle.sh
scripts/run_preview_hardware_profile.sh
scripts/verify_package_install.sh
```
