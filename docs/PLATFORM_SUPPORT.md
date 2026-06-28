# Platform Support Matrix

| Platform | RC4 status | Notes |
|---|---|---|
| Linux | Validated | Release build, CTest, examples, 100k stress, ASAN/UBSAN selected gates passed in this environment. |
| macOS | Expected | POSIX platform layer should apply; CI matrix includes macOS build/test. Not validated in this sandbox. |
| Windows | Compile-target / smoke pending | RC4 adds a Windows platform layer for append/flush/close/PID liveness and disables POSIX-only process-kill tests. Windows compile was not validated in this Linux sandbox. |

## POSIX-only tests

`graphenedb_rc_process_kill_tests` uses `fork`, `SIGKILL`, and `waitpid`; CMake excludes it on Windows.

## Remaining portability work

- Replace file-based lock creation with fully atomic cross-platform locking.
- Add Windows-specific crash process tests using `CreateProcess` + `TerminateProcess`.
- Add GitHub-hosted Windows CI evidence before claiming Windows support.
