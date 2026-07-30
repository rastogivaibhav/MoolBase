# GrapheneDB Full Source Restore Validation

## Authoritative source

Remote branch: `release/full-source-developer-preview`

Base commit: `e8829d184b28904e8908a63f3901b1609d1e8327`

The branch contains the GrapheneDB storage engine, HypoKosh, dialectic reasoning, governed learning, the HTTP pilot server, examples, scripts, documentation, and the associated tests directly as source files. No generic-data patch is required to obtain those components.

## Linux clean-source validation

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Result:

- full build passed
- `graphenedb_cli` built
- `graphenedb_server` built
- HypoKosh adapter tests passed
- dialectic tests passed
- governed-learning tests passed
- HTTP server contract tests passed
- 1M storage smoke passed
- CTest: 39/39 passed
- total test time: 19.40 seconds

## Scope correction

This restored branch represents the complete implementation at the authoritative source commit. It does not contain later experimental `reason-text`, `text_model_world`, or `recursive_model_world` work that existed only in subsequent local patch snapshots. Those features must not be advertised as part of this branch until they are ported into this tree and independently reproduced.

## Windows

An external Windows run identified a stale-lock read defect and applied a local correction to `src/db.cpp`. A candidate shared-read remediation is tracked separately and must pass a clean Windows build before merge. Windows support remains a merge gate.
