#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-launch-gate}"
REPORT_DIR="${REPORT_DIR:-$ROOT/reports/launch-gate}"
SOAK_SECONDS="${SOAK_SECONDS:-300}"
SOAK_CLIENTS="${SOAK_CLIENTS:-12}"
SOAK_RPS="${SOAK_RPS:-120}"
JOBS="${JOBS:-2}"

mkdir -p "$REPORT_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF
cmake --build "$BUILD_DIR" --target graphenedb_server graphenedb_healthcheck -j "$JOBS"

ctest --test-dir "$BUILD_DIR" --output-on-failure \
  -R 'graphenedb_tests|graphenedb_physical_lattice_primary_tests|graphenedb_rc5_fault_injection_tests|graphenedb_server_launch_hardening_tests|graphenedb_docker_security_static_tests' \
  | tee "$REPORT_DIR/ctest.txt"

python3 "$ROOT/scripts/server_launch_hardening_test.py" "$BUILD_DIR/graphenedb_server" \
  | tee "$REPORT_DIR/server_launch_hardening.json"
"$ROOT/scripts/run_adverse_filesystem_tests.sh" "$BUILD_DIR" \
  | tee "$REPORT_DIR/adverse_filesystem.txt"
python3 "$ROOT/scripts/validate_docker_security.py" "$ROOT" \
  | tee "$REPORT_DIR/docker_security_static.json"
python3 "$ROOT/scripts/server_soak_test.py" \
  --binary "$BUILD_DIR/graphenedb_server" \
  --seconds "$SOAK_SECONDS" \
  --clients "$SOAK_CLIENTS" \
  --target-rps "$SOAK_RPS" \
  --output "$REPORT_DIR/server_soak.json" \
  | tee "$REPORT_DIR/server_soak.txt"

echo "graphenedb_launch_readiness_gate_passed=true"
