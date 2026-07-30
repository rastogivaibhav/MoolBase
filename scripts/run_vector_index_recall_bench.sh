#!/usr/bin/env bash
set -euo pipefail

NODES="${NODES:-5000}"
QUERIES="${QUERIES:-200}"
DIM="${DIM:-32}"
K="${K:-10}"
INDEX="${INDEX:-auto}"
MIN_RECALL="${MIN_RECALL:-0.999}"
BUILD_DIR="${BUILD_DIR:-build-release}"
OUT="${OUT:-reports/VECTOR_INDEX_RECALL_OUTPUT.txt}"

cmake --build "$BUILD_DIR" --target graphenedb_vector_index_recall_bench --config Release

mkdir -p "$(dirname "$OUT")"
"$BUILD_DIR/graphenedb_vector_index_recall_bench" "$NODES" "$QUERIES" "$DIM" "$K" "$INDEX" "$MIN_RECALL" | tee "$OUT"
