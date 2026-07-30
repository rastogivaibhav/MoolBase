#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${GRAPHENEDB_VERIFY_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/graphenedb-install-XXXXXX")}" 
BUILD_DIR="${WORK_DIR}/build"
INSTALL_DIR="${WORK_DIR}/install"
CONSUMER_BUILD_DIR="${WORK_DIR}/consumer-build"
JOBS="${GRAPHENEDB_JOBS:-2}"
KEEP="${GRAPHENEDB_KEEP_VERIFY_DIR:-0}"

cleanup() {
  if [[ "${KEEP}" != "1" ]]; then
    rm -rf "${WORK_DIR}"
  else
    echo "verification files retained at ${WORK_DIR}"
  fi
}
trap cleanup EXIT

for command_name in cmake c++; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "error: '${command_name}' is required but was not found in PATH" >&2
    exit 2
  fi
done

printf '\n[1/5] Configuring a clean library build\n'
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=OFF \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"

printf '\n[2/5] Building GrapheneDB\n'
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

printf '\n[3/5] Installing GrapheneDB\n'
cmake --install "${BUILD_DIR}"

printf '\n[4/5] Building an unrelated consumer with find_package(GrapheneDB)\n'
cmake -S "${ROOT_DIR}/examples/installed_consumer" \
  -B "${CONSUMER_BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="${INSTALL_DIR}"
cmake --build "${CONSUMER_BUILD_DIR}" --parallel "${JOBS}"

CONSUMER="$(find "${CONSUMER_BUILD_DIR}" -type f -name graphenedb_installed_consumer -perm -111 | head -n 1 || true)"
if [[ -z "${CONSUMER}" ]]; then
  echo "error: installed-package consumer executable was not produced" >&2
  exit 3
fi

printf '\n[5/5] Running the installed-package consumer\n'
"${CONSUMER}"

printf '\nPASS: clean build, install, find_package and consumer execution succeeded.\n'
