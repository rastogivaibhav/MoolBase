#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
cmake -S . -B build-rc5 -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build-rc5 -j "${JOBS:-2}"
ctest --test-dir build-rc5 -R "graphenedb_(acid_lattice|lattice)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests" --output-on-failure
