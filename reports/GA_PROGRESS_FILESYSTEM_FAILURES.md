# GA Progress: Filesystem Failure Evidence

Date: 2026-07-04

## What changed

- Added POSIX CTest target `graphenedb_real_filesystem_failure_tests`.
- Added POSIX CTest target `graphenedb_real_filesystem_failure_tests`.
- The permission test exercises two real permission-denied cases:
  - reopen with a read-only `graphene.wal`
  - backup into a denied destination parent
- Added POSIX CTest target `graphenedb_disk_pressure_tests`.
- The disk-pressure test uses `RLIMIT_FSIZE` to force a real WAL append failure, verifies the failed write returns `IoError`, validates the live DB, reopens it, and proves the prior durable node remains visible.
- The test skips when run as root because root can bypass normal file permission checks.
- Documented the new gate in:
  - `docs/NEXT_GA_EXECUTION_PLAN.md`
  - `docs/GA_READINESS_SCORECARD.md`
  - `docs/RELEASE_CHECKLIST.md`

## Local verification

Windows configuration passed:

```text
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON
```

Focused build passed:

```text
cmake --build build-release --target graphenedb_rc5_fault_injection_tests graphenedb_c_api_tests graphenedb_tests --config Release
```

Focused CTest passed:

```text
ctest --test-dir build-release -R "graphenedb_rc5_fault_injection_tests|graphenedb_c_api_tests|^graphenedb_tests$" --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 3
```

## Remaining evidence

The new real filesystem tests must be run on a POSIX release/CI host as a non-root user:

```bash
ctest --test-dir build-release -R "graphenedb_real_filesystem_failure_tests|graphenedb_disk_pressure_tests" --output-on-failure
```

Enterprise GA still requires true volume-full or quota-backed target-filesystem testing, a 24-hour soak, multi-hour fuzzing, target-hardware performance reports, release signing, and final release-host recovery rehearsal.
