#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-/mnt/data/graphenedb_v1_rc4_portability_core_debt_pack.zip}"
cd "$ROOT/.."
rm -f "$OUT"
zip -qr "$OUT" graphenedb_v1 \
  -x 'graphenedb_v1/build-*/*' \
  -x 'graphenedb_v1/.git/*' \
  -x 'graphenedb_v1/**/*.o' \
  -x 'graphenedb_v1/**/*.a'
echo "$OUT"
