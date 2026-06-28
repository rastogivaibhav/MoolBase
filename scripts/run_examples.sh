#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
"$ROOT/scripts/build_release.sh" >/dev/null
for exe in graphenedb_uniqueness_demo graphenedb_api_coding_memory graphenedb_api_incident_memory graphenedb_api_team_brain; do
  echo "--- $exe ---"
  "$BUILD/$exe"
done
