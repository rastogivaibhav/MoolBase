#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${EVIDENCE_LAB_LIVE_BUILD_DIR:-$ROOT/build/evidence-lab-live-gate}"
VENV_DIR="${EVIDENCE_LAB_LIVE_VENV:-$BUILD_DIR/venv}"
DATA_DIR="${EVIDENCE_LAB_LIVE_DATA_DIR:-$BUILD_DIR/data}"
PORT="${EVIDENCE_LAB_LIVE_PORT:-18080}"
LOG="$BUILD_DIR/gateway.log"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$DATA_DIR"

cmake -S "$ROOT" -B "$BUILD_DIR/graphenedb" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=OFF \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF
cmake --build "$BUILD_DIR/graphenedb" --target graphenedb_server --parallel "${GRAPHENEDB_JOBS:-2}"

python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/pip" install -e "$ROOT/apps/evidence_lab/gateway[all]"

cleanup() {
  if [[ -n "${GATEWAY_PID:-}" ]]; then
    kill "$GATEWAY_PID" 2>/dev/null || true
    wait "$GATEWAY_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

EVIDENCE_LAB_BACKEND=live \
EVIDENCE_LAB_PUBLIC_MODE=false \
EVIDENCE_LAB_SAMPLES_DIR="$ROOT/apps/evidence_lab/samples" \
EVIDENCE_LAB_DATA_DIR="$DATA_DIR" \
EVIDENCE_LAB_ALLOWED_HOSTS="127.0.0.1,localhost" \
EVIDENCE_LAB_ALLOWED_ORIGINS="http://127.0.0.1:8088" \
GRAPHENEDB_SERVER_BINARY="$BUILD_DIR/graphenedb/graphenedb_server" \
GRAPHENEDB_SOURCE_COMMIT="$(git -C "$ROOT" rev-parse HEAD)" \
GRAPHENEDB_PUBLIC_VERSION="v0.6.0-alpha.1-live-gate" \
  "$VENV_DIR/bin/uvicorn" evidence_lab.main:app \
    --host 127.0.0.1 --port "$PORT" >"$LOG" 2>&1 &
GATEWAY_PID=$!

for attempt in $(seq 1 150); do
  if curl -fsS "http://127.0.0.1:$PORT/v1/public/health" >/dev/null; then
    break
  fi
  if ! kill -0 "$GATEWAY_PID" 2>/dev/null; then
    cat "$LOG"
    exit 1
  fi
  sleep 0.2
done

"$VENV_DIR/bin/python" "$ROOT/scripts/validate_evidence_lab_public.py" \
  "http://127.0.0.1:$PORT" \
  --expected-commit "$(git -C "$ROOT" rev-parse HEAD)"

printf '\nEVIDENCE_LAB_EXACT_HEAD_LIVE_GATE=PASS\n'
