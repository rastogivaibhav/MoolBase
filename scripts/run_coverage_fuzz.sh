#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPORTS="$ROOT/reports"
BUILD="${FUZZ_BUILD_DIR:-$ROOT/build_fuzz}"
OUT="${OUT:-$REPORTS/RC_COVERAGE_FUZZ_OUTPUT.txt}"
mkdir -p "$REPORTS"
: "${CC:=clang}"
: "${CXX:=clang++}"
TARGET_TRIPLE="$("$CXX" -dumpmachine 2>/dev/null || true)"
if [[ "$TARGET_TRIPLE" == *windows-gnu* ]]; then
  echo "libFuzzer is not supported by the current Clang target '$TARGET_TRIPLE'. Use a libFuzzer-capable Clang toolchain on Linux/WSL or a supported Windows Clang setup." >&2
  exit 2
fi
CC="$CC" CXX="$CXX" cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_FUZZERS=ON
cmake --build "$BUILD" -j"${JOBS:-2}"
mkdir -p "$(dirname "$OUT")"
"$BUILD/graphenedb_wal_fuzzer" -runs="${FUZZ_RUNS:-1000}" | tee "$OUT"
