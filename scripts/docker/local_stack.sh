#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COMPOSE_FILE="${ROOT_DIR}/docker-compose.local.yml"
EVIDENCE_DIR="${GRAPHENEDB_EVIDENCE_DIR_HOST:-${ROOT_DIR}/docker-evidence}"
COMMAND="${1:-help}"

require() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: '$1' is required" >&2
    exit 2
  }
}

require docker
if ! docker compose version >/dev/null 2>&1; then
  echo "error: Docker Compose v2 is required" >&2
  exit 2
fi

mkdir -p "${EVIDENCE_DIR}"
export GRAPHENEDB_SOURCE_COMMIT="${GRAPHENEDB_SOURCE_COMMIT:-$(git -C "${ROOT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)}"
export GRAPHENEDB_SOURCE_REF="${GRAPHENEDB_SOURCE_REF:-$(git -C "${ROOT_DIR}" branch --show-current 2>/dev/null || echo master)}"
export LOCAL_UID="${LOCAL_UID:-$(id -u 2>/dev/null || echo 1000)}"
export LOCAL_GID="${LOCAL_GID:-$(id -g 2>/dev/null || echo 1000)}"
export GRAPHENEDB_API_KEY="${GRAPHENEDB_API_KEY:-local-development-key}"

compose() {
  docker compose -f "${COMPOSE_FILE}" "$@"
}

case "${COMMAND}" in
  help)
    cat <<'HELP'
GrapheneDB local Docker commands

  build           Build the secure server and proof appliance images.
  up              Start the local GrapheneDB HTTP server on 127.0.0.1:8080.
  smoke           Start a clean server, import evidence and run the runtime API twice.
  proof-smoke     Run critical CTest, server contract, intervention and offline structural gates.
  proof-full      Run the full alpha gate plus the live server runtime contract.
  security        Run ASAN/UBSAN/TSAN and 10,000 libFuzzer executions.
  public          Download and run the frozen 2,500-record structural benchmark.
  down            Stop the stack without deleting the database volume.
  clean           Stop the stack and delete volumes, images and local evidence.

Evidence is written to ./docker-evidence. Set GRAPHENEDB_PLATFORM=linux/amd64
on Apple Silicon or another non-amd64 Docker host.
HELP
    ;;
  build)
    compose build graphenedb proof-smoke
    ;;
  up)
    compose up -d --build graphenedb
    compose ps
    ;;
  smoke)
    compose down -v --remove-orphans >/dev/null 2>&1 || true
    compose up -d --build graphenedb
    compose --profile smoke run --rm --build api-smoke
    compose ps
    ;;
  proof-smoke)
    compose --profile proof run --rm --build proof-smoke
    ;;
  proof-full)
    compose --profile proof run --rm --build proof-full
    ;;
  security)
    compose --profile security run --rm --build proof-security
    ;;
  public)
    compose --profile public run --rm --build proof-public
    ;;
  down)
    compose down --remove-orphans
    ;;
  clean)
    compose down -v --rmi local --remove-orphans || true
    rm -rf "${EVIDENCE_DIR}"
    ;;
  *)
    echo "error: unknown command '${COMMAND}'" >&2
    exit 2
    ;;
esac
