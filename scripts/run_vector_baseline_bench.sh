#!/usr/bin/env bash
set -euo pipefail

INCIDENTS="${INCIDENTS:-5000}"
QUERIES="${QUERIES:-200}"
DIM="${DIM:-64}"
BUILD_DIR="${BUILD_DIR:-build-release}"
OUT="${OUT:-reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt}"

cmake --build "$BUILD_DIR" --target graphenedb_vector_baseline_bench --config Release

mkdir -p "$(dirname "$OUT")"
"$BUILD_DIR/graphenedb_vector_baseline_bench" "$INCIDENTS" "$QUERIES" "$DIM" | tee "$OUT"
