# Platform Support Matrix

| Platform | RC5 status | Notes |
|---|---|---|
| Linux | CI target | Release build/test, package consumer smoke, sanitizer smoke, fuzz smoke, GA readiness smoke, and release-candidate bundle smoke are configured in GitHub Actions. Preserve CI artifacts before release. |
| macOS | CI target | POSIX platform layer should apply; CI matrix includes macOS build/test. Preserve CI artifacts before release. |
| Windows | Embedded/CLI smoke validated with policy caveat | Release builds, focused embedded/CLI CTest gates, operator flows, package verification, and release-manifest smoke have run locally. `graphenedb_cli_extract_tests` now uses a Python harness on Windows so the release-tree CTest suite can complete locally. The optional pilot server currently builds and validates on POSIX-oriented toolchains; in this snapshot a fresh Windows Clang/MinGW build of `graphenedb_server` fails on unguarded POSIX socket headers, so Windows server support should remain unclaimed until a portability pass lands. Local Windows Application Control can still block other newly linked executables, so full default gates must run on an approved release host. |

## POSIX-only tests

`graphenedb_rc_process_kill_tests` uses `fork`, `SIGKILL`, and `waitpid`; CMake excludes it on Windows.

## Remaining portability work

- Replace file-based lock creation with fully atomic cross-platform locking if stronger multi-process guarantees are required.
- Add Windows-specific crash process tests using `CreateProcess` + `TerminateProcess`.
- Preserve GitHub-hosted Windows CI evidence before claiming Windows release support.
