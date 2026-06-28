#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
"$ROOT/scripts/build_release.sh" >/dev/null
"$BUILD/graphenedb_rc_stress_tests" --incidents 33334 --queries 200 --dim 64 --reopen 1
