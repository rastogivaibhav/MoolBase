#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
NODES="${ONE_M_NODES:-1000000}"
QUERIES="${ONE_M_QUERIES:-20}"
DIM="${ONE_M_DIM:-64}"
VECTOR_INDEX="${ONE_M_VECTOR_INDEX:-auto}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-OFF}" "$ROOT/scripts/build_release.sh" >/dev/null
"$BUILD/graphenedb_rc_1m_storage_tests" --nodes "$NODES" --queries "$QUERIES" --dim "$DIM" --vector-index "$VECTOR_INDEX" --reopen 1
