#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RAW_BUILD_DIR="${BUILD_DIR:-build-rc5}"
VECTOR_INDEX="${VECTOR_INDEX:-auto}"
GRAPHENEDB_USE_FAISS_FLAG="${GRAPHENEDB_USE_FAISS:-OFF}"
case "$RAW_BUILD_DIR" in
  /*) BUILD_DIR="$RAW_BUILD_DIR" ;;
  *) BUILD_DIR="$ROOT/$RAW_BUILD_DIR" ;;
esac
OUT="${OUT:-}"
cd "$ROOT"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON -DGRAPHENEDB_USE_FAISS="$GRAPHENEDB_USE_FAISS_FLAG"
cmake --build "$BUILD_DIR" -j "${JOBS:-2}"
if [[ -n "$OUT" ]]; then
  mkdir -p "$(dirname "$OUT")"
  "$BUILD_DIR/graphenedb_extraction_ingest_bench" "${DOCS:-100}" "${NODES_PER_DOC:-50}" "${QUERIES:-100}" "${DIM:-64}" --vector-index "$VECTOR_INDEX" | tee "$OUT"
else
  "$BUILD_DIR/graphenedb_extraction_ingest_bench" "${DOCS:-100}" "${NODES_PER_DOC:-50}" "${QUERIES:-100}" "${DIM:-64}" --vector-index "$VECTOR_INDEX"
fi
