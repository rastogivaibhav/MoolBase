#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build_launchfix}"
cmake --build "$BUILD" --target graphenedb_real_filesystem_failure_tests graphenedb_disk_pressure_tests graphenedb_rc5_fault_injection_tests graphenedb_rc_process_kill_tests -j2
run_nonroot() {
  local exe="$1"
  if [[ "$(id -u)" == "0" ]] && command -v runuser >/dev/null 2>&1; then
    runuser -u nobody -- "$exe"
  else
    "$exe"
  fi
}
run_nonroot "$BUILD/graphenedb_real_filesystem_failure_tests"
run_nonroot "$BUILD/graphenedb_disk_pressure_tests"
"$BUILD/graphenedb_rc5_fault_injection_tests"
"$BUILD/graphenedb_rc_process_kill_tests"
echo "graphenedb_adverse_filesystem_suite_passed=true"
