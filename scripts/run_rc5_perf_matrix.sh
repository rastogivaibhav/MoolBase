#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
cmake -S . -B build-rc5 -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_BENCH=ON -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build-rc5 -j "${JOBS:-2}"
OUT="${OUT:-reports/RC5_STORAGE_RETRIEVAL_MATRIX_OUTPUT.txt}"
: > "$OUT"
CASES="${CASES:-2000:20:32 5000:30:64 10000:40:64}"
for case in $CASES; do
  IFS=: read -r nodes queries dim <<< "$case"
  {
    echo "=== case nodes=$nodes queries=$queries dim=$dim ==="
    "$ROOT/build-rc5/graphenedb_rc5_storage_retrieval_bench" "$nodes" "$queries" "$dim"
    echo
  } | tee -a "$OUT"
done
echo "$OUT"
