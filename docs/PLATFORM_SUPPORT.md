# Platform Support Matrix

| Platform | RC5 status | Notes |
|---|---|---|
| Linux | CI target | Release build/test, package consumer smoke, sanitizer smoke, fuzz smoke, GA readiness smoke, and release-candidate bundle smoke are configured in GitHub Actions. Preserve CI artifacts before release. |
| macOS | CI target | POSIX platform layer should apply; CI matrix includes macOS build/test. Preserve CI artifacts before release. |
| Windows | Embedded/CLI smoke validated with policy caveat | `GRAPHENEDB_BUILD_SERVER` defaults to `OFF`, so normal Windows builds contain the embedded library, CLI, tests, benchmarks, and examples without attempting the POSIX server. The optional pilot server remains unsupported on Windows and configuring it `ON` fails explicitly. `graphenedb_cli_extract_tests` uses a Python harness on Windows. Local Windows Application Control can still block newly linked executables, so full default gates must run on an approved release host. |

## POSIX-only tests

`graphenedb_rc_process_kill_tests` uses `fork`, `SIGKILL`, and `waitpid`; CMake excludes it on Windows.

## Remaining portability work

- Replace file-based lock creation with fully atomic cross-platform locking if stronger multi-process guarantees are required.
- Add Windows-specific crash process tests using `CreateProcess` + `TerminateProcess`.
- Preserve GitHub-hosted Windows CI evidence before claiming Windows release support.
