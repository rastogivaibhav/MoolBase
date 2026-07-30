#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-enterprise-ga}"
REPORT_DIR="${REPORT_DIR:-reports/enterprise-ga}"
CONFIG="${CONFIG:-Release}"
PROFILE_LABEL="${PROFILE_LABEL:-}"
APPROVED_HOST="${APPROVED_HOST:-0}"
SOAK_SECONDS="${SOAK_SECONDS:-300}"
SOAK_DIM="${SOAK_DIM:-32}"
STRESS_INCIDENTS="${STRESS_INCIDENTS:-16667}"
STRESS_QUERIES="${STRESS_QUERIES:-200}"
STRESS_DIM="${STRESS_DIM:-64}"
STRESS_VECTOR_INDEX="${STRESS_VECTOR_INDEX:-auto}"
ONE_M_NODES="${ONE_M_NODES:-1000000}"
ONE_M_QUERIES="${ONE_M_QUERIES:-5}"
ONE_M_DIM="${ONE_M_DIM:-64}"
ONE_M_VECTOR_INDEX="${ONE_M_VECTOR_INDEX:-auto}"
FUZZ_RUNS="${FUZZ_RUNS:-10000}"
FUZZ_BUILD_DIR="${FUZZ_BUILD_DIR:-build-enterprise-fuzz}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-0}"
SKIP_SOAK="${SKIP_SOAK:-0}"
SKIP_GA_READINESS="${SKIP_GA_READINESS:-0}"
SKIP_FULL_CTEST="${SKIP_FULL_CTEST:-0}"
SKIP_FUZZ="${SKIP_FUZZ:-0}"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
RUN_PATH="$ROOT/$REPORT_DIR/$STAMP"
SUMMARY="$RUN_PATH/ENTERPRISE_GA_SUMMARY.md"
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
# GrapheneDB Enterprise GA Campaign

- timestamp: $STAMP
- config: $CONFIG
- build_dir: $BUILD_PATH
- profile_label: $PROFILE_LABEL
- approved_host: $APPROVED_HOST
- soak_seconds: $SOAK_SECONDS
- soak_dim: $SOAK_DIM
- stress_incidents: $STRESS_INCIDENTS
- stress_queries: $STRESS_QUERIES
- stress_dim: $STRESS_DIM
- stress_vector_index: $STRESS_VECTOR_INDEX
- one_m_nodes: $ONE_M_NODES
- one_m_queries: $ONE_M_QUERIES
- one_m_dim: $ONE_M_DIM
- one_m_vector_index: $ONE_M_VECTOR_INDEX
- fuzz_runs: $FUZZ_RUNS
- graphenedb_use_faiss: $GRAPHENEDB_USE_FAISS
- skip_soak: $SKIP_SOAK
- skip_ga_readiness: $SKIP_GA_READINESS
- skip_full_ctest: $SKIP_FULL_CTEST
- skip_fuzz: $SKIP_FUZZ

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

run_optional_gate() {
  local name="$1"
  local log_name="$2"
  shift 2
  local log_path="$RUN_PATH/$log_name"
  {
    echo "## $name"
    echo "log: \`$log_path\`"
  } >> "$SUMMARY"
  if "$@" >"$log_path" 2>&1; then
    echo "- status: PASS" >> "$SUMMARY"
    echo >> "$SUMMARY"
    return 0
  else
    echo "- status: SKIP" >> "$SUMMARY"
    echo "- note: see $log_path" >> "$SUMMARY"
    echo >> "$SUMMARY"
    return 1
  fi
}

full_ctest_completed=true
[[ "$SKIP_FULL_CTEST" == "1" ]] && full_ctest_completed=false
ga_readiness_completed=true
[[ "$SKIP_GA_READINESS" == "1" ]] && ga_readiness_completed=false
fuzz_completed=true
[[ "$SKIP_FUZZ" == "1" ]] && fuzz_completed=false
soak_completed=true
[[ "$SKIP_SOAK" == "1" ]] && soak_completed=false
filesystem_gate_passed=false
disk_pressure_gate_passed=false
target_scale_dimensions_ready=false
[[ "${STRESS_DIM:-0}" -ge 384 && "${ONE_M_DIM:-0}" -ge 768 ]] && target_scale_dimensions_ready=true
full_day_soak_profile_ready=false
[[ "${SOAK_SECONDS:-0}" -ge 86400 ]] && full_day_soak_profile_ready=true

run_gate "host profile" "00-host-profile.log" \
  env OUT="$HOST_PROFILE" PROFILE_LABEL="$PROFILE_LABEL" INTENDED_HARDWARE=0 APPROVED_HOST="$APPROVED_HOST" \
  "$ROOT/scripts/write_host_profile.sh"

run_gate "configure" "01-configure.log" \
  cmake -S "$ROOT" -B "$BUILD_PATH" "-DCMAKE_BUILD_TYPE=$CONFIG" -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON "-DGRAPHENEDB_USE_FAISS=$( [[ "$GRAPHENEDB_USE_FAISS" == "1" || "$GRAPHENEDB_USE_FAISS" == "true" || "$GRAPHENEDB_USE_FAISS" == "TRUE" || "$GRAPHENEDB_USE_FAISS" == "on" || "$GRAPHENEDB_USE_FAISS" == "ON" ]] && echo ON || echo OFF )"

run_gate "build" "02-build.log" \
  cmake --build "$BUILD_PATH" -j"${JOBS:-2}"

if [[ "$SKIP_FULL_CTEST" == "1" ]]; then
  {
    echo "## full ctest suite"
    echo "log: \`$RUN_PATH/03-ctest.log\`"
    echo "- status: SKIP"
    echo "- note: skipped by caller"
    echo
  } >> "$SUMMARY"
else
  run_gate "full ctest suite" "03-ctest.log" \
    ctest --test-dir "$BUILD_PATH" --output-on-failure
fi

if [[ "$SKIP_GA_READINESS" == "1" ]]; then
  {
    echo "## ga readiness harness"
    echo "log: \`$RUN_PATH/04-ga-readiness.log\`"
    echo "- status: SKIP"
    echo "- note: skipped by caller"
    echo
  } >> "$SUMMARY"
else
  run_gate "ga readiness harness" "04-ga-readiness.log" \
    env BUILD_DIR="$BUILD_DIR" REPORT_DIR="reports/ga-readiness" CONFIG="$CONFIG" "$ROOT/scripts/run_ga_readiness.sh"
fi

run_gate "100k stress profile" "05-100k-stress.log" \
  env BUILD_DIR="$BUILD_PATH" GRAPHENEDB_USE_FAISS="$GRAPHENEDB_USE_FAISS" STRESS_INCIDENTS="$STRESS_INCIDENTS" STRESS_QUERIES="$STRESS_QUERIES" STRESS_DIM="$STRESS_DIM" STRESS_VECTOR_INDEX="$STRESS_VECTOR_INDEX" "$ROOT/scripts/run_100k_stress.sh"
{
  echo "- 100k_vector_index_requested: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" vector_index_requested)"
  echo "- 100k_vector_index: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" vector_index)"
  echo "- ingest_nodes_per_sec: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" ingest_nodes_per_sec)"
  echo "- causal_hit_rate: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" causal_hit_rate)"
  echo "- causal_p95_ms: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" causal_p95_ms)"
  echo "- metadata_service_p95_ms: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" metadata_service_p95_ms)"
  echo "- cross_layer_edges: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" cross_layer_edges)"
  echo "- defect_edges: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" defect_edges)"
  echo "- synthetic_edges: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" synthetic_edges)"
  echo "- reopen_ms: $(metric_value "$RUN_PATH/RC_STRESS_100K_OUTPUT.txt" reopen_ms)"
  echo
} >> "$SUMMARY"

run_gate "1m storage profile" "06-1m-storage.log" \
  env BUILD_DIR="$BUILD_PATH" GRAPHENEDB_USE_FAISS="$GRAPHENEDB_USE_FAISS" ONE_M_NODES="$ONE_M_NODES" ONE_M_QUERIES="$ONE_M_QUERIES" ONE_M_DIM="$ONE_M_DIM" ONE_M_VECTOR_INDEX="$ONE_M_VECTOR_INDEX" "$ROOT/scripts/run_1m_stress.sh"
{
  echo "- 1m_vector_index_requested: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" vector_index_requested)"
  echo "- 1m_vector_index: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" vector_index)"
  echo "- vector_p95_ms: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" vector_p95_ms)"
  echo "- causal_lattice_p95_ms: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" causal_lattice_p95_ms)"
  echo "- causal_root_hit_rate: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" causal_root_hit_rate)"
  echo "- metadata_service_p95_ms: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" metadata_service_p95_ms)"
  echo "- cross_layer_edges: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" cross_layer_edges)"
  echo "- defect_edges: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" defect_edges)"
  echo "- synthetic_edges: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" synthetic_edges)"
  echo "- reopen_ms: $(metric_value "$RUN_PATH/RC_STRESS_1M_STORAGE_OUTPUT.txt" reopen_ms)"
  echo
} >> "$SUMMARY"

if [[ "$SKIP_SOAK" == "1" ]]; then
  {
    echo "## soak gate"
    echo "log: \`$RUN_PATH/07-soak.log\`"
    echo "- status: SKIP"
    echo "- note: skipped by caller"
    echo
  } >> "$SUMMARY"
else
  run_gate "soak gate" "07-soak.log" \
    "$BUILD_PATH/graphenedb_rc_soak_tests" --seconds "$SOAK_SECONDS" --dim "$SOAK_DIM"
  cp "$RUN_PATH/07-soak.log" "$RUN_PATH/RC_SOAK_OUTPUT.txt"
fi

if run_optional_gate "real filesystem failure gates" "08-filesystem.log" \
  "$BUILD_PATH/graphenedb_real_filesystem_failure_tests"; then
  filesystem_gate_passed=true
fi
[[ -f "$RUN_PATH/08-filesystem.log" ]] && cp "$RUN_PATH/08-filesystem.log" "$RUN_PATH/RC_REAL_FILESYSTEM_FAILURES_OUTPUT.txt" || true

if run_optional_gate "disk pressure gates" "09-disk-pressure.log" \
  "$BUILD_PATH/graphenedb_disk_pressure_tests"; then
  disk_pressure_gate_passed=true
fi
[[ -f "$RUN_PATH/09-disk-pressure.log" ]] && cp "$RUN_PATH/09-disk-pressure.log" "$RUN_PATH/RC_DISK_PRESSURE_OUTPUT.txt" || true

if [[ "$SKIP_FUZZ" == "1" ]]; then
  {
    echo "## coverage fuzz gate"
    echo "log: \`$RUN_PATH/10-fuzz.log\`"
    echo "- status: SKIP"
    echo "- note: skipped by caller"
    echo
  } >> "$SUMMARY"
else
  run_gate "coverage fuzz gate" "10-fuzz.log" \
    env FUZZ_RUNS="$FUZZ_RUNS" FUZZ_BUILD_DIR="$ROOT/$FUZZ_BUILD_DIR" OUT="$RUN_PATH/RC_COVERAGE_FUZZ_OUTPUT.txt" "$ROOT/scripts/run_coverage_fuzz.sh"
fi

release_like_profile_ready=false
if [[ "$APPROVED_HOST" == "1" && "$full_day_soak_profile_ready" == "true" && "$soak_completed" == "true" && "$target_scale_dimensions_ready" == "true" && "$full_ctest_completed" == "true" && "$ga_readiness_completed" == "true" && "$fuzz_completed" == "true" && "$filesystem_gate_passed" == "true" && "$disk_pressure_gate_passed" == "true" ]]; then
  release_like_profile_ready=true
fi

{
  echo "- full_ctest_completed: $full_ctest_completed"
  echo "- ga_readiness_completed: $ga_readiness_completed"
  echo "- fuzz_completed: $fuzz_completed"
  echo "- soak_completed: $soak_completed"
  echo "- filesystem_gate_passed: $filesystem_gate_passed"
  echo "- disk_pressure_gate_passed: $disk_pressure_gate_passed"
  echo "- target_scale_dimensions_ready: $target_scale_dimensions_ready"
  echo "- full_day_soak_profile_ready: $full_day_soak_profile_ready"
  echo "- release_like_profile_ready: $release_like_profile_ready"
  echo
  echo "# Final Status"
  echo
  if [[ "$release_like_profile_ready" == "true" ]]; then
    echo "PASS"
  else
    echo "PARTIAL"
  fi
} >> "$SUMMARY"

echo "enterprise_ga_campaign=$RUN_PATH"
echo "$SUMMARY"
