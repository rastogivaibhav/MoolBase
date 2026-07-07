#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-package}"
INSTALL_DIR="${INSTALL_DIR:-build-package-install}"
CONSUMER_BUILD_DIR="${CONSUMER_BUILD_DIR:-build-package-consumer}"
CONFIG="${CONFIG:-Release}"

cmake -S "$ROOT" -B "$ROOT/$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DCMAKE_INSTALL_PREFIX="$ROOT/$INSTALL_DIR" \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON

cmake --build "$ROOT/$BUILD_DIR" -j "${JOBS:-2}"
cmake --install "$ROOT/$BUILD_DIR"

cmake -S "$ROOT/tests/package_consumer" -B "$ROOT/$CONSUMER_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DCMAKE_PREFIX_PATH="$ROOT/$INSTALL_DIR"

cmake --build "$ROOT/$CONSUMER_BUILD_DIR" -j "${JOBS:-2}"
"$ROOT/$CONSUMER_BUILD_DIR/graphenedb_package_consumer"
echo "graphenedb_package_install_verified=true"
