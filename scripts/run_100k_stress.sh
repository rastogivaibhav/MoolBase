#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
INCIDENTS="${STRESS_INCIDENTS:-16667}"
QUERIES="${STRESS_QUERIES:-200}"
DIM="${STRESS_DIM:-64}"
VECTOR_INDEX="${STRESS_VECTOR_INDEX:-auto}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-OFF}" "$ROOT/scripts/build_release.sh" >/dev/null
"$BUILD/graphenedb_rc_stress_tests" --incidents "$INCIDENTS" --queries "$QUERIES" --dim "$DIM" --vector-index "$VECTOR_INDEX" --reopen 1
