# Codex Repo Context

This file orients Codex-style coding agents working in GrapheneDB.

GrapheneDB v0.5.0 RC5 is an embedded C++20 causal/lattice-memory database. It stores memory nodes, vectors, metadata, snapshots, WAL/checkpoint state, causal and contradiction/supersession edges, and optional graphene-inspired lattice coordinates for explainable retrieval.

It is not a distributed service, SQL engine, auth system, vector-DB replacement, graph-DB replacement, or material-science simulator. Keep changes within the embedded library plus CLI product boundary unless explicitly asked to change that boundary.

## Start Here

Read these before making broad changes:

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/NEXT_GA_EXECUTION_PLAN.md`
- `reports/GA_STATUS_REPORT.md`

For specific domains:

- Lattice model: `docs/GRAPHENE_LATTICE_MODEL.md`, `docs/LATTICE_RETRIEVAL.md`
- Extraction ingestion: `docs/EXTRACTION_INGESTION.md`
- Durable format: `docs/STORAGE_FORMAT.md`
- Packaging: `docs/PACKAGING_DISTRIBUTION.md`
- Release evidence: `docs/GA_READINESS_VERIFICATION.md`, `reports/RELEASE_CANDIDATE_BUNDLE.md`

## Project Layout

- `include/graphene/` - public API headers.
- `src/` - implementation.
- `tools/graphenedb_cli.cpp` - CLI entry point.
- `tests/` - regression and release-gate tests.
- `bench/` - benchmark executables.
- `scripts/` - repeatable release, benchmark, package, and evidence workflows.
- `reports/` - preserved release evidence.

## Development Guidance

- Use existing APIs and local patterns before adding new abstractions.
- Keep storage compatibility in mind. Update `docs/STORAGE_FORMAT.md` and tests when durable records change.
- Keep `put_batch()` and `put_extraction()` idempotence/source-scoping behavior stable unless a task explicitly asks to change the contract.
- Preserve CLI JSON output stability for automation commands.
- Do not add broad product claims. Public-preview and enterprise-GA status must match `reports/GA_STATUS_REPORT.md`.
- Do not commit build directories, `graphify-out/`, Python bytecode, or historical scratch evidence. Commit only intentional release evidence.

## Immediate Next Step For Handoff

The RC5 developer-preview branch has been prepared and pushed as `codex/rc5-developer-preview`.

The next agent should do exactly this:

1. Open a GitHub pull request from `codex/rc5-developer-preview` into `master`.
2. In the PR description, state that this is a public developer-preview candidate, not enterprise GA.
3. Link the current evidence: `reports/GA_STATUS_REPORT.md`, `reports/preview-hardware/20260707-141656/PREVIEW_HARDWARE_SUMMARY.md`, `reports/ga-readiness/20260707-142146/GA_READINESS_SUMMARY.md`, and `reports/ga-evidence/20260707-142233.zip`.
4. Wait for CI. If CI fails, fix only the failing gate or portability issue. Do not add new features during PR stabilization.
5. If CI passes and review is acceptable, merge to `master`.
6. After merge, create the developer-preview tag or release using the label in `docs/GA_READINESS_SCORECARD.md`: `GrapheneDB v0.5.0-rc5 - embedded causal/lattice-memory DB for controlled pilots`.

Do not start enterprise-GA work in this PR. Enterprise GA remains a separate follow-up track: approved-host full readiness, 24-hour soak, multi-hour fuzzing, target-scale profiles, real filesystem failure evidence, signing/license review, and final vector-backend decisions.

## Verification

For a normal Windows release smoke:

```powershell
.\scripts\run_release_candidate_bundle.ps1
```

For intended preview performance evidence:

```powershell
.\scripts\run_preview_hardware_profile.ps1 -ProfileLabel developer-preview -IntendedHardware
```

For package consumption:

```powershell
.\scripts\verify_package_install.ps1
```

On this Windows host, local Application Control may block some freshly linked test executables. The RC bundle documents the local-policy CTest exclusion it uses; approved release hosts should rerun the full default gates without local exclusions.
