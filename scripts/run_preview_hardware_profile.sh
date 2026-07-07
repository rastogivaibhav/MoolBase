#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-preview-profile}"
REPORT_DIR="${REPORT_DIR:-reports/preview-hardware}"
CONFIG="${CONFIG:-Release}"
PROFILE_LABEL="${PROFILE_LABEL:-}"
INTENDED_HARDWARE="${INTENDED_HARDWARE:-0}"
VECTOR_BASELINE_INCIDENTS="${VECTOR_BASELINE_INCIDENTS:-5000}"
VECTOR_BASELINE_QUERIES="${VECTOR_BASELINE_QUERIES:-200}"
VECTOR_BASELINE_DIM="${VECTOR_BASELINE_DIM:-64}"
VECTOR_INDEX_NODES="${VECTOR_INDEX_NODES:-5000}"
VECTOR_INDEX_QUERIES="${VECTOR_INDEX_QUERIES:-200}"
VECTOR_INDEX_DIM="${VECTOR_INDEX_DIM:-32}"
VECTOR_INDEX_K="${VECTOR_INDEX_K:-10}"
VECTOR_INDEX="${VECTOR_INDEX:-auto}"
VECTOR_INDEX_MIN_RECALL="${VECTOR_INDEX_MIN_RECALL:-0.999}"
EXTRACTION_DOCS="${EXTRACTION_DOCS:-100}"
EXTRACTION_NODES_PER_DOC="${EXTRACTION_NODES_PER_DOC:-50}"
EXTRACTION_QUERIES="${EXTRACTION_QUERIES:-100}"
EXTRACTION_DIM="${EXTRACTION_DIM:-64}"
EXTRACTION_VECTOR_INDEX="${EXTRACTION_VECTOR_INDEX:-auto}"
STORAGE_NODES="${STORAGE_NODES:-20000}"
STORAGE_QUERIES="${STORAGE_QUERIES:-100}"
STORAGE_DIM="${STORAGE_DIM:-64}"
STORAGE_VECTOR_INDEX="${STORAGE_VECTOR_INDEX:-auto}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-0}"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
RUN_PATH="$ROOT/$REPORT_DIR/$STAMP"
SUMMARY="$RUN_PATH/PREVIEW_HARDWARE_SUMMARY.md"
BUILD_PATH="$ROOT/$BUILD_DIR"
HOST_PROFILE="$RUN_PATH/HOST_PROFILE.json"

metric_value() {
  local path="$1"
  local key="$2"
  python3 - "$path" "$key" <<'PY'
import pathlib
import re
import sys

path, key = sys.argv[1:]
text = pathlib.Path(path).read_text(encoding="utf-8", errors="ignore")
match = re.search(r'(?:^|\s)' + re.escape(key) + r'=([^\s]+)', text)
print(match.group(1) if match else "")
PY
}

mkdir -p "$RUN_PATH"

cat > "$SUMMARY" <<EOF
# GrapheneDB Preview Hardware Profile

- timestamp: $STAMP
- config: $CONFIG
- build_dir: $BUILD_PATH
- profile_label: $PROFILE_LABEL
- intended_hardware: $INTENDED_HARDWARE
- graphenedb_use_faiss: $GRAPHENEDB_USE_FAISS
- vector_baseline: incidents=$VECTOR_BASELINE_INCIDENTS queries=$VECTOR_BASELINE_QUERIES dim=$VECTOR_BASELINE_DIM
- vector_index: nodes=$VECTOR_INDEX_NODES queries=$VECTOR_INDEX_QUERIES dim=$VECTOR_INDEX_DIM k=$VECTOR_INDEX_K index=$VECTOR_INDEX min_recall=$VECTOR_INDEX_MIN_RECALL
- extraction: docs=$EXTRACTION_DOCS nodes_per_doc=$EXTRACTION_NODES_PER_DOC queries=$EXTRACTION_QUERIES dim=$EXTRACTION_DIM index=$EXTRACTION_VECTOR_INDEX
- storage: nodes=$STORAGE_NODES queries=$STORAGE_QUERIES dim=$STORAGE_DIM index=$STORAGE_VECTOR_INDEX

EOF

run_gate() {
  local name="$1"
  local log_name="$2"
  shift 2
  local log_path="$RUN_PATH/$log_name"
  {
    echo "## $name"
    echo "log: \`$log_path\`"
  } >> "$SUMMARY"
  if "$@" >"$log_path" 2>&1; then
    {
      echo "- status: PASS"
      echo
    } >> "$SUMMARY"
  else
    {
      echo "- status: FAIL"
      echo "- error: see $log_path"
      echo
    } >> "$SUMMARY"
    tail -n 80 "$log_path" || true
    return 1
  fi
}

run_gate "configure" "01-configure.log" \
  cmake -S "$ROOT" -B "$BUILD_PATH" "-DCMAKE_BUILD_TYPE=$CONFIG" -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON -DGRAPHENEDB_USE_FAISS="$( [[ "$GRAPHENEDB_USE_FAISS" == "1" ]] && echo ON || echo OFF )"

run_gate "host profile" "00-host-profile.log" \
  env OUT="$HOST_PROFILE" PROFILE_LABEL="$PROFILE_LABEL" INTENDED_HARDWARE="$INTENDED_HARDWARE" APPROVED_HOST=0 \
  "$ROOT/scripts/write_host_profile.sh"

run_gate "build benchmark targets" "02-build.log" \
  cmake --build "$BUILD_PATH" --target graphenedb_vector_baseline_bench graphenedb_vector_index_recall_bench graphenedb_extraction_ingest_bench graphenedb_rc5_storage_retrieval_bench -j"${JOBS:-2}"

run_gate "vector baseline comparison" "03-vector-baseline.log" \
  env INCIDENTS="$VECTOR_BASELINE_INCIDENTS" QUERIES="$VECTOR_BASELINE_QUERIES" DIM="$VECTOR_BASELINE_DIM" BUILD_DIR="$BUILD_PATH" OUT="$RUN_PATH/VECTOR_BASELINE_COMPARISON_OUTPUT.txt" \
  "$ROOT/scripts/run_vector_baseline_bench.sh"
{
  echo "- vector_root_hit_rate: $(metric_value "$RUN_PATH/VECTOR_BASELINE_COMPARISON_OUTPUT.txt" vector_root_hit_rate)"
  echo "- causal_root_hit_rate: $(metric_value "$RUN_PATH/VECTOR_BASELINE_COMPARISON_OUTPUT.txt" causal_root_hit_rate)"
  echo "- causal_p95_ms: $(metric_value "$RUN_PATH/VECTOR_BASELINE_COMPARISON_OUTPUT.txt" causal_p95_ms)"
  echo
} >> "$SUMMARY"

run_gate "vector index recall" "04-vector-index.log" \
  env NODES="$VECTOR_INDEX_NODES" QUERIES="$VECTOR_INDEX_QUERIES" DIM="$VECTOR_INDEX_DIM" K="$VECTOR_INDEX_K" INDEX="$VECTOR_INDEX" MIN_RECALL="$VECTOR_INDEX_MIN_RECALL" BUILD_DIR="$BUILD_PATH" OUT="$RUN_PATH/VECTOR_INDEX_RECALL_OUTPUT.txt" \
  "$ROOT/scripts/run_vector_index_recall_bench.sh"
{
  echo "- mean_recall_at_k: $(metric_value "$RUN_PATH/VECTOR_INDEX_RECALL_OUTPUT.txt" mean_recall_at_k)"
  echo "- worst_recall_at_k: $(metric_value "$RUN_PATH/VECTOR_INDEX_RECALL_OUTPUT.txt" worst_recall_at_k)"
  echo "- vector_index_resolved: $(metric_value "$RUN_PATH/VECTOR_INDEX_RECALL_OUTPUT.txt" vector_index)"
  echo
} >> "$SUMMARY"

run_gate "extraction ingest performance" "05-extraction.log" \
  env DOCS="$EXTRACTION_DOCS" NODES_PER_DOC="$EXTRACTION_NODES_PER_DOC" QUERIES="$EXTRACTION_QUERIES" DIM="$EXTRACTION_DIM" VECTOR_INDEX="$EXTRACTION_VECTOR_INDEX" GRAPHENEDB_USE_FAISS="$( [[ "$GRAPHENEDB_USE_FAISS" == "1" ]] && echo ON || echo OFF )" BUILD_DIR="$BUILD_PATH" OUT="$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" \
  "$ROOT/scripts/run_extraction_ingest_bench.sh"
{
  echo "- extraction_vector_index_requested: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" vector_index_requested)"
  echo "- extraction_vector_index: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" vector_index)"
  echo "- extract_nodes_per_sec: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" extract_nodes_per_sec)"
  echo "- extract_doc_p95_ms: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" extract_doc_p95_ms)"
  echo "- causal_lattice_p95_ms: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" causal_lattice_p95_ms)"
  echo "- causal_root_hit_rate: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" causal_root_hit_rate)"
  echo "- metadata_doc_p95_ms: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" metadata_doc_p95_ms)"
  echo "- avg_lattice_neighbors: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" avg_lattice_neighbors)"
  echo "- reopen_ms: $(metric_value "$RUN_PATH/EXTRACTION_INGEST_OUTPUT.txt" reopen_ms)"
  echo
} >> "$SUMMARY"

run_gate "storage retrieval performance" "06-storage.log" \
  env NODES="$STORAGE_NODES" QUERIES="$STORAGE_QUERIES" DIM="$STORAGE_DIM" VECTOR_INDEX="$STORAGE_VECTOR_INDEX" GRAPHENEDB_USE_FAISS="$( [[ "$GRAPHENEDB_USE_FAISS" == "1" ]] && echo ON || echo OFF )" BUILD_DIR="$BUILD_PATH" OUT="$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" \
  "$ROOT/scripts/run_rc5_storage_retrieval_bench.sh"
{
  echo "- storage_vector_index_requested: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" vector_index_requested)"
  echo "- storage_vector_index: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" vector_index)"
  echo "- ingest_nodes_per_sec: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" ingest_nodes_per_sec)"
  echo "- vector_p95_ms: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" vector_p95_ms)"
  echo "- causal_lattice_p95_ms: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" causal_lattice_p95_ms)"
  echo "- causal_root_hit_rate: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" causal_root_hit_rate)"
  echo "- metadata_service_p95_ms: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" metadata_service_p95_ms)"
  echo "- avg_lattice_neighbors: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" avg_lattice_neighbors)"
  echo "- cross_layer_edges: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" cross_layer_edges)"
  echo "- reopen_ms: $(metric_value "$RUN_PATH/RC5_STORAGE_RETRIEVAL_OUTPUT.txt" reopen_ms)"
  echo
} >> "$SUMMARY"

{
  echo "# Final Status"
  echo
  echo "PASS"
} >> "$SUMMARY"

echo "preview_hardware_profile=$RUN_PATH"
echo "$SUMMARY"
