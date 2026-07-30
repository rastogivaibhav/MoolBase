#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_TAG="${IMAGE_TAG:-graphenedb:soak-candidate}"
REPORT_ROOT="${REPORT_ROOT:-$ROOT/reports/container-soaks}"
API_KEY="${GRAPHENEDB_API_KEY:-soak-test-key}"
JOBS="${JOBS:-2}"

SOAK24_NAME="${SOAK24_NAME:-graphenedb-soak-24h}"
SOAK72_NAME="${SOAK72_NAME:-graphenedb-soak-72h}"
SOAK24_VOLUME="${SOAK24_VOLUME:-graphenedb-soak-24h-data}"
SOAK72_VOLUME="${SOAK72_VOLUME:-graphenedb-soak-72h-data}"
SOAK24_PORT="${SOAK24_PORT:-18080}"
SOAK72_PORT="${SOAK72_PORT:-28080}"

SOAK24_SECONDS="${SOAK24_SECONDS:-86400}"
SOAK72_SECONDS="${SOAK72_SECONDS:-259200}"
SOAK24_CLIENTS="${SOAK24_CLIENTS:-8}"
SOAK72_CLIENTS="${SOAK72_CLIENTS:-8}"
SOAK24_RPS="${SOAK24_RPS:-80}"
SOAK72_RPS="${SOAK72_RPS:-80}"

KEEP_CONTAINERS="${KEEP_CONTAINERS:-0}"
KEEP_VOLUMES="${KEEP_VOLUMES:-0}"

SOAK24_REPORT="$REPORT_ROOT/24h"
SOAK72_REPORT="$REPORT_ROOT/72h"

mkdir -p "$SOAK24_REPORT" "$SOAK72_REPORT"

command -v docker >/dev/null 2>&1 || {
  echo "docker is required for the parallel container soak runner" >&2
  exit 2
}

command -v python3 >/dev/null 2>&1 || {
  echo "python3 is required for the soak client harness" >&2
  exit 2
}

cleanup() {
  local status=$?
  collect_artifacts "$SOAK24_NAME" "$SOAK24_REPORT" || true
  collect_artifacts "$SOAK72_NAME" "$SOAK72_REPORT" || true
  if [[ "$KEEP_CONTAINERS" != "1" ]]; then
    docker rm -f "$SOAK24_NAME" "$SOAK72_NAME" >/dev/null 2>&1 || true
  fi
  if [[ "$KEEP_VOLUMES" != "1" ]]; then
    docker volume rm "$SOAK24_VOLUME" "$SOAK72_VOLUME" >/dev/null 2>&1 || true
  fi
  exit "$status"
}
trap cleanup EXIT

wait_for_container_health() {
  local name="$1"
  for _ in $(seq 1 240); do
    local health
    health="$(docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "$name" 2>/dev/null || true)"
    if [[ "$health" == "healthy" ]]; then
      return 0
    fi
    if [[ "$health" == "exited" || "$health" == "dead" ]]; then
      echo "container $name is not healthy; current state=$health" >&2
      docker logs "$name" >&2 || true
      return 1
    fi
    sleep 2
  done
  echo "timed out waiting for container $name to become healthy" >&2
  docker logs "$name" >&2 || true
  return 1
}

collect_artifacts() {
  local name="$1"
  local report_dir="$2"
  if docker inspect "$name" >/dev/null 2>&1; then
    docker inspect "$name" > "$report_dir/container-inspect.json" || true
    docker logs --timestamps "$name" > "$report_dir/container.log" 2>&1 || true
    docker exec "$name" /bin/sh -lc 'ls -lah /var/lib/graphenedb' > "$report_dir/db-files.txt" 2>&1 || true
  fi
}

start_container() {
  local name="$1"
  local volume="$2"
  local port="$3"

  docker rm -f "$name" >/dev/null 2>&1 || true
  if [[ "$KEEP_VOLUMES" != "1" ]]; then
    docker volume rm "$volume" >/dev/null 2>&1 || true
  fi
  docker volume create "$volume" >/dev/null

  docker run -d \
    --name "$name" \
    -p "${port}:8080" \
    -e "GRAPHENEDB_API_KEY=$API_KEY" \
    --mount "source=$volume,target=/var/lib/graphenedb" \
    --read-only \
    --tmpfs /tmp:size=64m,mode=1777 \
    --cap-drop ALL \
    --security-opt no-new-privileges:true \
    --pids-limit 256 \
    --memory 2g \
    --cpus 2.0 \
    "$IMAGE_TAG" >/dev/null

  wait_for_container_health "$name"
}

echo "Building container image $IMAGE_TAG"
docker build --pull -t "$IMAGE_TAG" "$ROOT" | tee "$REPORT_ROOT/docker-build.txt"

echo "Starting $SOAK24_NAME on port $SOAK24_PORT"
start_container "$SOAK24_NAME" "$SOAK24_VOLUME" "$SOAK24_PORT"

echo "Starting $SOAK72_NAME on port $SOAK72_PORT"
start_container "$SOAK72_NAME" "$SOAK72_VOLUME" "$SOAK72_PORT"

echo "Launching parallel soak clients"
python3 "$ROOT/scripts/server_soak_test.py" \
  --base-url "http://127.0.0.1:$SOAK24_PORT" \
  --api-key "$API_KEY" \
  --container-name "$SOAK24_NAME" \
  --seconds "$SOAK24_SECONDS" \
  --clients "$SOAK24_CLIENTS" \
  --target-rps "$SOAK24_RPS" \
  --output "$SOAK24_REPORT/server_soak.json" \
  | tee "$SOAK24_REPORT/server_soak.txt" &
PID24=$!

python3 "$ROOT/scripts/server_soak_test.py" \
  --base-url "http://127.0.0.1:$SOAK72_PORT" \
  --api-key "$API_KEY" \
  --container-name "$SOAK72_NAME" \
  --seconds "$SOAK72_SECONDS" \
  --clients "$SOAK72_CLIENTS" \
  --target-rps "$SOAK72_RPS" \
  --output "$SOAK72_REPORT/server_soak.json" \
  | tee "$SOAK72_REPORT/server_soak.txt" &
PID72=$!

FAIL=0
wait "$PID24" || FAIL=1
wait "$PID72" || FAIL=1

collect_artifacts "$SOAK24_NAME" "$SOAK24_REPORT"
collect_artifacts "$SOAK72_NAME" "$SOAK72_REPORT"

if [[ "$FAIL" != "0" ]]; then
  echo "One or more parallel soaks failed" >&2
  exit 1
fi

echo "graphenedb_parallel_container_soaks_passed=true"
