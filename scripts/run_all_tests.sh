#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
"$ROOT/scripts/build_release.sh"
ctest --test-dir "$BUILD" --output-on-failure
