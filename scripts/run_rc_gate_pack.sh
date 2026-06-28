#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
REPORTS="$ROOT/reports"
mkdir -p "$REPORTS"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"${JOBS:-2}"
ctest --test-dir "$BUILD" --output-on-failure | tee "$REPORTS/RC_CTEST_RELEASE_OUTPUT.txt"
"$BUILD/graphenedb_rc_stress_tests" --incidents "${STRESS_INCIDENTS:-33334}" --queries "${STRESS_QUERIES:-200}" --dim "${STRESS_DIM:-64}" --reopen 1 | tee "$REPORTS/RC_STRESS_100K_OUTPUT.txt"
set +e
cmake -S "$ROOT" -B "$ROOT/build_faiss_gate" -DGRAPHENEDB_USE_FAISS=ON > "$REPORTS/RC_FAISS_OPTION_OUTPUT.txt" 2>&1
code=$?
echo "exit_code=$code" >> "$REPORTS/RC_FAISS_OPTION_OUTPUT.txt"
set -e
if [ "$code" -eq 0 ]; then
  echo "ERROR: GRAPHENEDB_USE_FAISS=ON unexpectedly succeeded. Expected fail-loudly when FAISS is unavailable." >&2
  exit 2
fi
