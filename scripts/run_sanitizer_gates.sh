#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPORTS="$ROOT/reports"
mkdir -p "$REPORTS"
cmake -S "$ROOT" -B "$ROOT/build_asan" -DCMAKE_BUILD_TYPE=Debug -DGRAPHENEDB_BUILD_BENCH=OFF -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build "$ROOT/build_asan" -j"${JOBS:-2}"
{
  ctest --test-dir "$ROOT/build_asan" -R 'graphenedb_tests|graphenedb_rc_crash_tests|graphenedb_rc_fuzz_tests|graphenedb_rc_kosh_adapter_tests|graphenedb_rc_lock_rotation_metadata_tests|graphenedb_rc_process_kill_tests|graphenedb_rc_real_kosh_adapter_tests|graphenedb_rc_1m_storage_smoke' --output-on-failure
  "$ROOT/build_asan/graphenedb_rc_stress_tests" --incidents "${ASAN_STRESS_INCIDENTS:-500}" --queries "${ASAN_STRESS_QUERIES:-20}" --dim "${ASAN_STRESS_DIM:-32}" --reopen 1
} | tee "$REPORTS/RC_ASAN_UBSAN_OUTPUT.txt"
cmake -S "$ROOT" -B "$ROOT/build_tsan" -DCMAKE_BUILD_TYPE=Debug -DGRAPHENEDB_BUILD_BENCH=OFF -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer'
cmake --build "$ROOT/build_tsan" -j"${JOBS:-2}"
{
  ctest --test-dir "$ROOT/build_tsan" -R 'graphenedb_tests|graphenedb_rc_crash_tests|graphenedb_rc_fuzz_tests|graphenedb_rc_kosh_adapter_tests|graphenedb_rc_lock_rotation_metadata_tests|graphenedb_rc_process_kill_tests|graphenedb_rc_real_kosh_adapter_tests|graphenedb_rc_1m_storage_smoke' --output-on-failure
  "$ROOT/build_tsan/graphenedb_rc_stress_tests" --incidents "${TSAN_STRESS_INCIDENTS:-200}" --queries "${TSAN_STRESS_QUERIES:-10}" --dim "${TSAN_STRESS_DIM:-16}" --reopen 1
} | tee "$REPORTS/RC_TSAN_OUTPUT.txt"
