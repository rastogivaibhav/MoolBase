#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
"$ROOT/scripts/build_release.sh" >/dev/null
"$BUILD/graphenedb_rc_1m_storage_tests" --nodes 1000000 --queries 20 --dim 2 --reopen 1
