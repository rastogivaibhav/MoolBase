# GrapheneDB P0 Remediation Specification

Status: implemented and locally validated for the v0.6.0-rc1 correctness branch.

## Objective

Strengthen the controlled-pilot release without adding product features or
changing the canonical storage/WAL record formats. This pass addresses defects
that can invalidate release evidence, single-writer safety, recovery behavior,
or retry semantics.

## Scope

This remediation implements:

1. assertions that remain active in Release test builds;
2. an explicit optional-server build switch, disabled by default on Windows;
3. atomic cross-platform lock-file creation;
4. durable temporary-file replacement and WAL truncation helpers;
5. strict distinction between a torn final WAL fragment and corruption in a
   complete frame;
6. pre-commit validation of physical-primary coordinates and layers;
7. committed-write success even when post-commit sidecar/checkpoint maintenance
   fails, with the maintenance failure exposed by `inspect()`;
8. enforcement of `DBOptions::create_if_missing`;
9. API/security documentation reconciliation; and
10. regression tests for the above contracts.

This pass does not introduce distributed operation, SQL, an embedding service,
new reasoning features, or a new durable record format.

## Required invariants

### Test evidence

- A test assertion must execute in Debug and Release configurations.
- CI must fail during compilation if a test target is built with `NDEBUG`.
- Release evidence must identify the configuration and test count.

### Single-writer exclusion

- Lock creation is one atomic filesystem operation.
- Two concurrent openers cannot both report a successful writable open.
- Stale-lock recovery may remove a dead owner's lock, but the subsequent create
  must still be atomic.

### Commit and maintenance

- A write is committed when its complete transaction, including `COMMIT`, has
  been appended and flushed according to `fsync_on_commit`.
- Failures before that point return an error and publish no in-memory mutation.
- Failures rebuilding a derived lattice sidecar or performing automatic WAL
  rotation after that point must not report the committed write as absent.
- Such failures set `maintenance_required=true` and a diagnostic message in
  `inspect()`. Explicit `compact()` still reports its own failure.

### Checkpoint replacement

- A checkpoint temporary file is flushed before replacement.
- Replacement is atomic on the supported platform.
- The containing directory is flushed where the platform provides that
  primitive.
- WAL truncation is flushed before success is returned.
- The canonical data/WAL files remain the recovery source of truth.

### WAL recovery

- An unterminated invalid final fragment may be ignored as a torn tail.
- A recovered torn tail is durably truncated to the last valid frame before the
  WAL is reopened for append.
- An invalid checksum or malformed complete frame is corruption.
- An invalid frame followed by additional bytes/frames is corruption.

### Physical lattice

- Physical-primary storage accepts non-negative layers only.
- Coordinates must fit within `physical_lattice_radius` before the WAL commit.
- Distinct accepted coordinates must map to distinct physical ordinals.

### Open behavior

- If `create_if_missing=false` and the directory does not exist, `open()` fails
  and does not create it.

## Verification

Required fast gate:

```bash
cmake -S . -B build-remediation \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF
cmake --build build-remediation -j2
ctest --test-dir build-remediation --output-on-failure -j2
```

POSIX release hosts must additionally build with
`-DGRAPHENEDB_BUILD_SERVER=ON` and run the server pilot contract.

## Local validation result

The implementation was validated on Windows with the embedded library and CLI
surfaces:

- A clean Release build completed with tests enabled and the optional POSIX
  HTTP server disabled.
- All 26 registered CTest tests passed with assertions explicitly kept active.
- The WAL fuzz regression passed three consecutive runs after the
  durable-replace retry correction.
- A default Windows configure and build succeeded without requiring an explicit
  server option.
- The OpenAPI source-surface contract test passed and verifies all implemented
  server routes plus the lattice-hop limit.

The POSIX server build and live HTTP contract remain release-host verification
items because the controlled-pilot server is intentionally unsupported on
Windows.

## Deferred roadmap

The following remain separate, versioned projects:

- page-oriented binary canonical storage and incremental compaction;
- persistent metadata/external-ID/vector indexes;
- background/RCU vector-index generations instead of search-time full rebuilds;
- calibrated causal path selection and confidence;
- embedding model/version contracts and a first-class causal-search HTTP API;
- snapshot retention epochs across compaction;
- admin-plane authorization and backup-root policy;
- complete JSON/OpenAPI code generation; and
- 24/72-hour soak, multi-hour fuzzing, signed OCI/SBOM evidence, and final
  licence selection.
