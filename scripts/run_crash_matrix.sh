#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
"$ROOT/scripts/build_release.sh" >/dev/null
"$BUILD/graphenedb_rc_process_kill_tests"
"$BUILD/graphenedb_rc_crash_tests"
