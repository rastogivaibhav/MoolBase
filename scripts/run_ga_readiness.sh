#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-ga-readiness}"
REPORT_DIR="${REPORT_DIR:-reports/ga-readiness}"
CONFIG="${CONFIG:-Release}"
PROFILE_LABEL="${PROFILE_LABEL:-}"
APPROVED_HOST="${APPROVED_HOST:-0}"
CTEST_EXCLUDE="${CTEST_EXCLUDE:-graphenedb_rc_(fuzz|kosh_adapter)_tests}"
FOCUSED_REGEX="${FOCUSED_REGEX:-graphenedb_(acid_lattice|lattice|disk_pressure|real_filesystem_failure)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests}"
VECTOR_INDEX_RECALL_KIND="${VECTOR_INDEX_RECALL_KIND:-auto}"
VECTOR_INDEX_RECALL_MIN="${VECTOR_INDEX_RECALL_MIN:-0.999}"
VECTOR_INDEX_RECALL_NODES="${VECTOR_INDEX_RECALL_NODES:-5000}"
VECTOR_INDEX_RECALL_QUERIES="${VECTOR_INDEX_RECALL_QUERIES:-200}"
VECTOR_INDEX_RECALL_DIM="${VECTOR_INDEX_RECALL_DIM:-32}"
VECTOR_INDEX_RECALL_K="${VECTOR_INDEX_RECALL_K:-10}"
EXTRACTION_DOCS="${EXTRACTION_DOCS:-10}"
EXTRACTION_NODES_PER_DOC="${EXTRACTION_NODES_PER_DOC:-20}"
EXTRACTION_QUERIES="${EXTRACTION_QUERIES:-10}"
EXTRACTION_VECTOR_INDEX="${EXTRACTION_VECTOR_INDEX:-auto}"
STORAGE_NODES="${STORAGE_NODES:-2000}"
STORAGE_QUERIES="${STORAGE_QUERIES:-20}"
DIM="${DIM:-32}"
STORAGE_VECTOR_INDEX="${STORAGE_VECTOR_INDEX:-auto}"
SKIP_PACKAGE="${SKIP_PACKAGE:-0}"
EXTRACTION_THRESHOLDS="${EXTRACTION_THRESHOLDS:-}"
STORAGE_THRESHOLDS="${STORAGE_THRESHOLDS:-}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-0}"

STAMP="$(date +%Y%m%d-%H%M%S)"
RUN_DIR="$ROOT/$REPORT_DIR/$STAMP"
SUMMARY="$RUN_DIR/GA_READINESS_SUMMARY.md"
HOST_PROFILE="$RUN_DIR/HOST_PROFILE.json"
mkdir -p "$RUN_DIR"
SUMMARY_LINES=()

add_summary_line() {
  SUMMARY_LINES+=("$1")
}

write_summary() {
  local temp_summary="${SUMMARY}.tmp"
  printf '%s\n' "${SUMMARY_LINES[@]}" > "$temp_summary"
  mv -f "$temp_summary" "$SUMMARY"
}

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

add_summary_line "# GrapheneDB GA Readiness Run"
add_summary_line ""
add_summary_line "- timestamp: $STAMP"
add_summary_line "- config: $CONFIG"
add_summary_line "- profile_label: $PROFILE_LABEL"
add_summary_line "- approved_host: $APPROVED_HOST"
add_summary_line "- ctest_exclude: $CTEST_EXCLUDE"
add_summary_line "- focused_regex: $FOCUSED_REGEX"
add_summary_line "- vector_index_recall_kind: $VECTOR_INDEX_RECALL_KIND"
add_summary_line "- vector_index_recall_min: $VECTOR_INDEX_RECALL_MIN"
add_summary_line "- vector_index_recall_nodes: $VECTOR_INDEX_RECALL_NODES"
add_summary_line "- vector_index_recall_queries: $VECTOR_INDEX_RECALL_QUERIES"
add_summary_line "- vector_index_recall_dim: $VECTOR_INDEX_RECALL_DIM"
add_summary_line "- vector_index_recall_k: $VECTOR_INDEX_RECALL_K"
add_summary_line "- extraction_docs: $EXTRACTION_DOCS"
add_summary_line "- extraction_nodes_per_doc: $EXTRACTION_NODES_PER_DOC"
add_summary_line "- extraction_queries: $EXTRACTION_QUERIES"
add_summary_line "- extraction_vector_index: $EXTRACTION_VECTOR_INDEX"
add_summary_line "- storage_nodes: $STORAGE_NODES"
add_summary_line "- storage_queries: $STORAGE_QUERIES"
add_summary_line "- dim: $DIM"
add_summary_line "- storage_vector_index: $STORAGE_VECTOR_INDEX"
add_summary_line "- graphenedb_use_faiss: $GRAPHENEDB_USE_FAISS"
add_summary_line "- extraction_thresholds: $EXTRACTION_THRESHOLDS"
add_summary_line "- storage_thresholds: $STORAGE_THRESHOLDS"
add_summary_line ""
write_summary

run_gate() {
  local name="$1"
  local log="$2"
  shift 2
  local log_path="$RUN_DIR/$log"
  add_summary_line "## $name"
  add_summary_line "log: \`$log_path\`"
  write_summary
  if "$@" > "$log_path" 2>&1; then
    add_summary_line "- status: PASS"
    add_summary_line ""
    write_summary
  else
    local rc=$?
    add_summary_line "- status: FAIL"
    add_summary_line "- exit_code: $rc"
    add_summary_line ""
    write_summary
    tail -80 "$log_path" || true
    exit "$rc"
  fi
}

run_gate "host profile" "00-host-profile.log" \
  env OUT="$HOST_PROFILE" PROFILE_LABEL="$PROFILE_LABEL" INTENDED_HARDWARE=0 APPROVED_HOST="$APPROVED_HOST" \
  "$ROOT/scripts/write_host_profile.sh"

run_gate "configure" "01-configure.log" \
  cmake -S "$ROOT" -B "$ROOT/$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DGRAPHENEDB_BUILD_TESTS=ON \
    -DGRAPHENEDB_BUILD_BENCH=ON \
    -DGRAPHENEDB_BUILD_EXAMPLES=ON \
    -DGRAPHENEDB_USE_FAISS="$( [[ "$GRAPHENEDB_USE_FAISS" == "1" ]] && echo ON || echo OFF )"

run_gate "build" "02-build.log" cmake --build "$ROOT/$BUILD_DIR" -j "${JOBS:-2}"

run_gate "ctest runnable suite" "03-ctest.log" \
  ctest --test-dir "$ROOT/$BUILD_DIR" -E "$CTEST_EXCLUDE" --output-on-failure

run_gate "focused acid/crash gates" "04-acid-crash.log" \
  ctest --test-dir "$ROOT/$BUILD_DIR" -R "$FOCUSED_REGEX" --output-on-failure

if [[ "$SKIP_PACKAGE" != "1" ]]; then
  run_gate "package install consumer" "05-package.log" "$ROOT/scripts/verify_package_install.sh"
fi

run_gate "recovery rehearsal" "04b-recovery-rehearsal.log" \
  env CLI="$ROOT/$BUILD_DIR/graphenedb_cli" OUT="$RUN_DIR/RECOVERY_REHEARSAL_OUTPUT.txt" \
  "$ROOT/scripts/run_recovery_rehearsal.sh"

run_gate "kosh adapter gate" "04c-kosh-adapter.log" \
  env BUILD_DIR="$BUILD_DIR" OUT="reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt" \
  "$ROOT/scripts/run_kosh_adapter_gate.sh"

run_gate "vector index recall benchmark" "06-vector-index-recall.log" \
  "$ROOT/$BUILD_DIR/graphenedb_vector_index_recall_bench" \
    "$VECTOR_INDEX_RECALL_NODES" "$VECTOR_INDEX_RECALL_QUERIES" "$VECTOR_INDEX_RECALL_DIM" "$VECTOR_INDEX_RECALL_K" "$VECTOR_INDEX_RECALL_KIND" "$VECTOR_INDEX_RECALL_MIN"
add_summary_line "- vector_index_recall_requested: $(metric_value "$RUN_DIR/06-vector-index-recall.log" vector_index_requested)"
add_summary_line "- vector_index_recall: $(metric_value "$RUN_DIR/06-vector-index-recall.log" vector_index)"
add_summary_line "- vector_index_recall_mean_recall_at_k: $(metric_value "$RUN_DIR/06-vector-index-recall.log" mean_recall_at_k)"
add_summary_line ""
write_summary

run_gate "extraction ingest benchmark" "07-extraction-bench.log" \
  "$ROOT/$BUILD_DIR/graphenedb_extraction_ingest_bench" "$EXTRACTION_DOCS" "$EXTRACTION_NODES_PER_DOC" "$EXTRACTION_QUERIES" "$DIM" --vector-index "$EXTRACTION_VECTOR_INDEX"
add_summary_line "- extraction_vector_index_requested: $(metric_value "$RUN_DIR/07-extraction-bench.log" vector_index_requested)"
add_summary_line "- extraction_vector_index: $(metric_value "$RUN_DIR/07-extraction-bench.log" vector_index)"
add_summary_line ""
write_summary

if [[ -n "$EXTRACTION_THRESHOLDS" ]]; then
  # shellcheck disable=SC2206
  extraction_rules=($EXTRACTION_THRESHOLDS)
  run_gate "extraction benchmark thresholds" "07b-extraction-thresholds.log" \
    python3 "$ROOT/scripts/check_benchmark_thresholds.py" "$RUN_DIR/07-extraction-bench.log" "${extraction_rules[@]}"
fi

run_gate "storage retrieval benchmark" "08-storage-bench.log" \
  "$ROOT/$BUILD_DIR/graphenedb_rc5_storage_retrieval_bench" "$STORAGE_NODES" "$STORAGE_QUERIES" "$DIM" --vector-index "$STORAGE_VECTOR_INDEX"
add_summary_line "- storage_vector_index_requested: $(metric_value "$RUN_DIR/08-storage-bench.log" vector_index_requested)"
add_summary_line "- storage_vector_index: $(metric_value "$RUN_DIR/08-storage-bench.log" vector_index)"
add_summary_line ""
write_summary

if [[ -n "$STORAGE_THRESHOLDS" ]]; then
  # shellcheck disable=SC2206
  storage_rules=($STORAGE_THRESHOLDS)
  run_gate "storage benchmark thresholds" "08b-storage-thresholds.log" \
    python3 "$ROOT/scripts/check_benchmark_thresholds.py" "$RUN_DIR/08-storage-bench.log" "${storage_rules[@]}"
fi

add_summary_line "# Final Status"
add_summary_line ""
add_summary_line "PASS"
write_summary

echo "graphenedb_ga_readiness_passed=true"
echo "$SUMMARY"
