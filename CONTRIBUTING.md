# Contributing

GrapheneDB is currently an RC-stage experimental database core. Contributions should preserve the safety-first posture.

## Local validation before a PR

Run:

```bash
./scripts/build_release.sh
./scripts/run_all_tests.sh
./scripts/run_graphene_uniqueness_demo.sh
```

For storage/search changes, also run:

```bash
./scripts/run_sanitizers.sh
./scripts/run_crash_matrix.sh
```

For performance-sensitive changes, run:

```bash
./scripts/run_100k_stress.sh
```

## Contribution rules

- Do not weaken validation to make tests pass.
- Do not silently ignore WAL corruption.
- Do not add placeholder replay data.
- Do not allow vector dimension mismatches.
- Do not store invalid edges.
- Add tests for every new storage or recovery behaviour.
- Update docs when changing user-visible API or durability semantics.

## Coding style

- C++20.
- Prefer explicit `Status` returns for recoverable errors.
- Keep public API in `include/graphene`.
- Keep examples small and runnable.
