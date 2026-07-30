#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-offline}"
case "$MODE" in
  offline|public|all) ;;
  *) echo "usage: $0 [offline|public|all]" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/cross_dataset_portable"
REPORTS="$ROOT/reports/cross_dataset/portable"
EVIDENCE="$REPORTS/evidence"
CXX="${CXX:-g++}"
PYTHON="${PYTHON:-python3}"
BABI_PER_TASK="${BABI_PER_TASK:-200}"
HOTPOT="${HOTPOT:-500}"
FEVER_PER_LABEL="${FEVER_PER_LABEL:-200}"
SEED="${CROSS_DATASET_SEED:-20260729}"

mkdir -p "$BUILD" "$REPORTS" "$EVIDENCE"

for command in "$CXX" "$PYTHON"; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "required command not found: $command" >&2
    exit 3
  }
done

BINARY="$BUILD/bench_cross_dataset_epistemic"
echo "[1/6] Compiling actual FiberBundleBuilder and LyapunovCritic"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -I"$ROOT/include" \
  "$ROOT/bench/bench_cross_dataset_epistemic.cpp" \
  "$ROOT/src/fiber_bundle.cpp" \
  "$ROOT/src/stability_critic.cpp" \
  -o "$BINARY"

run_suite() {
  local label="$1"
  local input="$2"
  local prefix="$REPORTS/$label"
  echo "Running $label: $input"
  "$BINARY" "$input" "${prefix}_results.csv"
  "$PYTHON" "$ROOT/benchmarks/cross_dataset/summarise_cross_dataset.py" \
    --input "${prefix}_results.csv" \
    --json-output "${prefix}_summary.json" \
    --markdown-output "${prefix}_summary.md" \
    --enforce
}

echo "[2/6] Enforcing committed source-isolated regression"
run_suite "source_isolated" "$ROOT/benchmarks/cross_dataset/source_isolated_actual.tsv"

PUBLIC_INPUT="$BUILD/examples.tsv"
if [[ "$MODE" == "public" || "$MODE" == "all" ]]; then
  echo "[3/6] Downloading and normalising frozen public benchmark records"
  "$PYTHON" "$ROOT/benchmarks/cross_dataset/prepare_cross_dataset.py" \
    --output "$PUBLIC_INPUT" \
    --babi-per-task "$BABI_PER_TASK" \
    --hotpot "$HOTPOT" \
    --fever-per-label "$FEVER_PER_LABEL" \
    --seed "$SEED"

  echo "[4/6] Enforcing public benchmark gates"
  run_suite "public" "$PUBLIC_INPUT"
else
  echo "[3/6] Public download skipped (mode=$MODE)"
  echo "[4/6] Public benchmark skipped (mode=$MODE)"
fi

INPUTS=(
  "bench/bench_cross_dataset_epistemic.cpp"
  "benchmarks/cross_dataset/source_isolated_actual.tsv"
  "benchmarks/cross_dataset/summarise_cross_dataset.py"
  "include/graphene/fiber_bundle.hpp"
  "include/graphene/stability_critic.hpp"
  "src/fiber_bundle.cpp"
  "src/stability_critic.cpp"
  "reports/cross_dataset/portable/source_isolated_results.csv"
  "reports/cross_dataset/portable/source_isolated_summary.json"
)
if [[ -f "$PUBLIC_INPUT" ]]; then
  INPUTS+=(
    "build/cross_dataset_portable/examples.tsv"
    "build/cross_dataset_portable/examples.manifest.json"
    "reports/cross_dataset/portable/public_results.csv"
    "reports/cross_dataset/portable/public_summary.json"
  )
fi

CAPTURE_ARGS=()
for input in "${INPUTS[@]}"; do CAPTURE_ARGS+=(--input "$input"); done

echo "[5/6] Capturing environment and immutable evidence"
"$PYTHON" "$ROOT/scripts/capture_cross_dataset_environment.py" \
  --repo "$ROOT" \
  --output "$EVIDENCE/environment.json" \
  --mode "$MODE" \
  --compiler "$(command -v "$CXX")" \
  "${CAPTURE_ARGS[@]}"

git -C "$ROOT" rev-parse HEAD > "$EVIDENCE/source_commit.txt" 2>/dev/null || echo "unknown" > "$EVIDENCE/source_commit.txt"
cp "$REPORTS/source_isolated_results.csv" "$REPORTS/source_isolated_summary.json" "$REPORTS/source_isolated_summary.md" "$EVIDENCE/"
if [[ -f "$REPORTS/public_results.csv" ]]; then
  cp "$REPORTS/public_results.csv" "$REPORTS/public_summary.json" "$REPORTS/public_summary.md" "$EVIDENCE/"
  cp "$PUBLIC_INPUT" "$PUBLIC_INPUT.manifest.json" "$EVIDENCE/"
fi

(
  cd "$EVIDENCE"
  find . -type f ! -name checksums.sha256 -print0 | sort -z | xargs -0 sha256sum > checksums.sha256
)

echo "[6/6] Packaging evidence"
ARCHIVE="$REPORTS/cross-dataset-${MODE}-$(tr -d '\r\n' < "$EVIDENCE/source_commit.txt" | cut -c1-12).tar.gz"
tar -C "$EVIDENCE" -czf "$ARCHIVE" .

echo "portable_cross_dataset_status=PASS"
echo "mode=$MODE"
echo "evidence=$EVIDENCE"
echo "archive=$ARCHIVE"
