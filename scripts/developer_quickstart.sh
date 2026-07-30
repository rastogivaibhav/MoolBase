#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${GRAPHENEDB_BUILD_DIR:-${ROOT_DIR}/build/quickstart}"
JOBS="${GRAPHENEDB_JOBS:-2}"

for command_name in cmake c++; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "error: '${command_name}' is required but was not found in PATH" >&2
    exit 2
  fi
done

printf '\n[1/3] Configuring GrapheneDB\n'
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=OFF \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON

printf '\n[2/3] Building the reasoning demo\n'
cmake --build "${BUILD_DIR}" \
  --target graphenedb_hypokosh_runtime_demo \
  --parallel "${JOBS}"

DEMO="$(find "${BUILD_DIR}" -type f -name graphenedb_hypokosh_runtime_demo -perm -111 | head -n 1 || true)"
if [[ -z "${DEMO}" ]]; then
  echo "error: graphenedb_hypokosh_runtime_demo was not produced" >&2
  exit 3
fi

printf '\n[3/3] Running the demo\n'
"${DEMO}"

cat <<EOF_MESSAGE

GrapheneDB is ready to explore.

Next steps:
  Full tests:       cmake --build "${BUILD_DIR}" --parallel ${JOBS} && ctest --test-dir "${BUILD_DIR}" --output-on-failure
  Install check:    scripts/verify_developer_install.sh
  C++ API example:  examples/installed_consumer/main.cpp
  Runtime guide:    docs/DEVELOPER_QUICKSTART.md
EOF_MESSAGE
