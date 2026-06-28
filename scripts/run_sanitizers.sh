#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${JOBS:-2}"
ASAN_BUILD="${ASAN_BUILD_DIR:-$ROOT/build-asan-ubsan}"
cmake -S "$ROOT" -B "$ASAN_BUILD" -DCMAKE_BUILD_TYPE=Debug -DGRAPHENEDB_BUILD_EXAMPLES=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build "$ASAN_BUILD" -j"$JOBS" --target graphenedb_tests graphenedb_rc_crash_tests graphenedb_rc_fuzz_tests graphenedb_rc_lock_rotation_metadata_tests graphenedb_rc_real_kosh_adapter_tests
ctest --test-dir "$ASAN_BUILD" --output-on-failure -R "graphenedb_tests|graphenedb_rc_crash_tests|graphenedb_rc_fuzz_tests|graphenedb_rc_lock_rotation_metadata_tests|graphenedb_rc_real_kosh_adapter_tests"

TSAN_BUILD="${TSAN_BUILD_DIR:-$ROOT/build-tsan}"
cmake -S "$ROOT" -B "$TSAN_BUILD" -DCMAKE_BUILD_TYPE=Debug -DGRAPHENEDB_BUILD_EXAMPLES=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
cmake --build "$TSAN_BUILD" -j"$JOBS" --target graphenedb_tests graphenedb_rc_lock_rotation_metadata_tests graphenedb_rc_soak_tests
ctest --test-dir "$TSAN_BUILD" --output-on-failure -R "graphenedb_tests|graphenedb_rc_lock_rotation_metadata_tests|graphenedb_rc_soak_tests"
