#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPORTS="$ROOT/reports"
BUILD="$ROOT/build_fuzz"
mkdir -p "$REPORTS"
: "${CC:=clang}"
: "${CXX:=clang++}"
CC="$CC" CXX="$CXX" cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_FUZZERS=ON
cmake --build "$BUILD" -j"${JOBS:-2}"
"$BUILD/graphenedb_wal_fuzzer" -runs="${FUZZ_RUNS:-1000}" | tee "$REPORTS/RC_COVERAGE_FUZZ_OUTPUT.txt"
