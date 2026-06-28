#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FUZZ_BUILD="${FUZZ_BUILD_DIR:-$ROOT/build-fuzz}"
if ! command -v clang++ >/dev/null 2>&1; then
  echo "clang++ is required for LLVM libFuzzer" >&2
  exit 3
fi
cmake -S "$ROOT" -B "$FUZZ_BUILD" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_EXAMPLES=OFF -DGRAPHENEDB_BUILD_FUZZERS=ON
cmake --build "$FUZZ_BUILD" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
"$FUZZ_BUILD/graphenedb_wal_fuzzer" -runs="${RUNS:-1000}"
