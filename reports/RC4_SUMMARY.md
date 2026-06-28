# GrapheneDB v1 RC4 Portability + Core Debt Summary

## Scope

RC4 turns RC3 from a Linux-heavy controlled-pilot pack into a cleaner developer-review candidate by addressing the specific review feedback.

## Added / changed

- Platform abstraction: `include/graphene/platform.hpp`
- POSIX implementation: `src/platform_posix.cpp`
- Windows implementation: `src/platform_windows.cpp`
- Vector index interface: `include/graphene/vector_index.hpp`
- FlatVectorIndex implementation
- Named retrieval tuning constants via `RetrievalTuning`
- Manifest-persisted `next_txid`
- Cached current-snapshot live node/edge counts
- `std::popcount` instead of `__builtin_popcountll`
- POSIX-only process-kill test guard in CMake
- Lock recovery test no longer depends on `unistd.h`
- README platform/support update
- CI matrix update for Linux/macOS/Windows release build
- RC4 reviewer-response report

## Local validation

```text
Release build: PASS
CTest: 10/10 PASS
Examples/demo: PASS
100k stress: PASS
ASAN/UBSAN selected gates: PASS
TSAN: not completed in this sandbox due timeout
```

## Maturity movement

RC3 rating from reviewer: 7.1/10.

RC4 likely moves the codebase toward ~7.5/10 for controlled-pilot/developer-review readiness because it reduces portability and maintainability debt. It does not yet change enterprise-GA readiness materially because the core scale/indexing and long-running validation gaps remain.
