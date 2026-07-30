#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${DIALECTIC_INTERVENTION_BUILD_DIR:-${ROOT_DIR}/build/dialectic-intervention}"
REPORT_DIR="${DIALECTIC_INTERVENTION_REPORT_DIR:-${ROOT_DIR}/reports/dialectic_intervention}"
REPETITIONS="${DIALECTIC_INTERVENTION_REPETITIONS:-20}"
JOBS="${GRAPHENEDB_JOBS:-2}"

mkdir -p "${REPORT_DIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=OFF \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF
cmake --build "${BUILD_DIR}" \
  --target graphenedb_dialectic_intervention_bench \
  --parallel "${JOBS}"

BENCH="$(find "${BUILD_DIR}" -type f -name graphenedb_dialectic_intervention_bench -perm -111 | head -n 1 || true)"
if [[ -z "${BENCH}" ]]; then
  echo "error: intervention benchmark executable was not produced" >&2
  exit 3
fi

"${BENCH}" "${REPETITIONS}" "${REPORT_DIR}/results.csv" \
  | tee "${REPORT_DIR}/console_summary.txt"
python3 "${ROOT_DIR}/benchmarks/dialectic_intervention/summarise.py" \
  --input "${REPORT_DIR}/results.csv" \
  --json-output "${REPORT_DIR}/summary.json" \
  --markdown-output "${REPORT_DIR}/summary.md" \
  --enforce
