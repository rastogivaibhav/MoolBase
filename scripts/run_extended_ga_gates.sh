#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPORTS="$ROOT/reports"
BUILD="$ROOT/build_ga_gates"
mkdir -p "$REPORTS"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"${JOBS:-2}"
ctest --test-dir "$BUILD" --output-on-failure | tee "$REPORTS/RC_FINAL_CTEST_OUTPUT.txt"
"$BUILD/graphenedb_rc_1m_storage_tests" --nodes "${ONE_M_NODES:-1000000}" --queries "${ONE_M_QUERIES:-5}" --dim "${ONE_M_DIM:-2}" --reopen 1 | tee "$REPORTS/RC_STRESS_1M_STORAGE_OUTPUT.txt"
"$BUILD/graphenedb_rc_process_kill_tests" | tee "$REPORTS/RC_PROCESS_KILL_MATRIX_OUTPUT.txt"
"$BUILD/graphenedb_rc_lock_rotation_metadata_tests" | tee "$REPORTS/RC_WAL_ROTATION_METADATA_LOCK_OUTPUT.txt"
"$BUILD/graphenedb_rc_real_kosh_adapter_tests" | tee "$REPORTS/RC_REAL_KOSH_ADAPTER_OUTPUT.txt"
"$BUILD/graphenedb_rc_soak_tests" --seconds "${SOAK_SECONDS:-60}" --dim "${SOAK_DIM:-16}" | tee "$REPORTS/RC_SOAK_OUTPUT.txt"
