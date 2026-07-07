#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-release}"
OUT="${OUT:-reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt}"

cmake --build "$ROOT/$BUILD_DIR" --target graphenedb_rc_real_kosh_adapter_tests --config Release

mkdir -p "$(dirname "$OUT")"
"$ROOT/$BUILD_DIR/graphenedb_rc_real_kosh_adapter_tests" | tee "$OUT"
