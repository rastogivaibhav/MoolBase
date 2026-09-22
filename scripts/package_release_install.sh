#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-release-package}"
INSTALL_DIR="${INSTALL_DIR:-build-release-install}"
OUT="${1:-$ROOT/graphenedb-install-package.zip}"

cd "$ROOT"
cmake -S "$ROOT" -B "$ROOT/$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ROOT/$INSTALL_DIR" \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build "$ROOT/$BUILD_DIR" -j "${JOBS:-2}"
cmake --install "$ROOT/$BUILD_DIR"

BUILD_DIR=build-release-package-verify \
INSTALL_DIR="$INSTALL_DIR" \
CONSUMER_BUILD_DIR=build-release-package-consumer \
  bash "$ROOT/scripts/verify_package_install.sh"

rm -f "$OUT"
if command -v zip >/dev/null 2>&1; then
  (cd "$ROOT" && zip -qr "$OUT" "$INSTALL_DIR")
else
  tar -C "$ROOT" -czf "${OUT%.zip}.tar.gz" "$INSTALL_DIR"
  OUT="${OUT%.zip}.tar.gz"
fi

"$ROOT/scripts/write_release_manifest.sh" "$OUT" "$ROOT/$INSTALL_DIR"
echo "$OUT"
