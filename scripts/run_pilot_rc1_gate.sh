#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-pilot-rc1}"
REPORT_DIR="${REPORT_DIR:-$ROOT/reports/pilot-rc1/gate}"
PACKAGE_BUILD_DIR="${PACKAGE_BUILD_DIR:-build-pilot-package}"
PACKAGE_INSTALL_DIR="${PACKAGE_INSTALL_DIR:-build-pilot-install}"
PACKAGE_CONSUMER_DIR="${PACKAGE_CONSUMER_DIR:-build-pilot-consumer}"
JOBS="${JOBS:-2}"
SOAK_SECONDS="${SOAK_SECONDS:-30}"
SOAK_CLIENTS="${SOAK_CLIENTS:-6}"
SOAK_RPS="${SOAK_RPS:-40}"

mkdir -p "$REPORT_DIR"

{
  echo "graphenedb_pilot_gate_started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "graphenedb_pilot_gate_build_dir=$BUILD_DIR"
  echo "graphenedb_pilot_gate_soak_seconds=$SOAK_SECONDS"
} | tee "$REPORT_DIR/environment.txt"

cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF \
  2>&1 | tee "$REPORT_DIR/cmake_configure.txt"
cmake --build "$BUILD_DIR" -j "$JOBS" 2>&1 | tee "$REPORT_DIR/cmake_build.txt"

ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$JOBS" \
  2>&1 | tee "$REPORT_DIR/ctest_all.txt"

python3 - "$ROOT/docs/api/openapi-v1.yaml" <<'PY' \
  2>&1 | tee "$REPORT_DIR/openapi_validation.txt"
import sys
from pathlib import Path
import yaml

path = Path(sys.argv[1])
doc = yaml.safe_load(path.read_text(encoding="utf-8"))
assert doc.get("openapi") == "3.0.3"
assert doc.get("info", {}).get("title") == "GrapheneDB Pilot API"
expected_paths = {
    "/v1/health", "/v1/ready", "/v1/version", "/v1/nodes",
    "/v1/nodes/bulk", "/v1/extractions", "/v1/nodes/{nodeId}",
    "/v1/facts", "/v1/edges",
    "/v1/edges/provenance", "/v1/search/hybrid", "/v1/search/lattice",
    "/v1/retrieve/bundle", "/v1/retrieve/explain", "/v1/retrieve/temporal",
    "/v1/reason/dialectic", "/v1/reason/hypokosh",
    "/v1/learning/episodes", "/v1/learning/episodes/quarantine",
    "/v1/learning/policies/evaluate",
    "/v1/learning/policies/decisions",
    "/v1/learning/policies/current",
    "/v1/metrics", "/v1/metrics/prometheus", "/v1/admin/capacity",
    "/v1/admin/validate", "/v1/admin/checkpoint", "/v1/admin/compact",
    "/v1/admin/inspect", "/v1/admin/backup",
    "/v1/admin/validate/provenance", "/v1/admin/lattice/quality",
}
assert set(doc.get("paths", {})) == expected_paths
assert (
    doc["paths"]["/v1/search/lattice"]["get"]["parameters"][1]["schema"]["maximum"]
    == 16
)
print(f"openapi_valid=true paths={len(doc.get('paths', {}))}")
PY

python3 "$ROOT/scripts/server_pilot_contract_test.py" "$BUILD_DIR/graphenedb_server" \
  2>&1 | tee "$REPORT_DIR/server_pilot_contract.json"

python3 "$ROOT/scripts/server_learning_contract_test.py" "$BUILD_DIR/graphenedb_server" \
  2>&1 | tee "$REPORT_DIR/server_learning_contract.txt"

python3 -u "$ROOT/scripts/server_soak_test.py" \
  --binary "$BUILD_DIR/graphenedb_server" \
  --seconds "$SOAK_SECONDS" \
  --clients "$SOAK_CLIENTS" \
  --target-rps "$SOAK_RPS" \
  --output "$REPORT_DIR/server_soak.json" \
  2>&1 | tee "$REPORT_DIR/server_soak.txt"

BUILD_DIR="$PACKAGE_BUILD_DIR" \
INSTALL_DIR="$PACKAGE_INSTALL_DIR" \
CONSUMER_BUILD_DIR="$PACKAGE_CONSUMER_DIR" \
JOBS="$JOBS" \
  bash "$ROOT/scripts/verify_package_install.sh" \
  2>&1 | tee "$REPORT_DIR/package_consumer.txt"

{
  echo "graphenedb_pilot_gate_finished_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "graphenedb_pilot_rc1_gate_passed=true"
} | tee "$REPORT_DIR/result.txt"
