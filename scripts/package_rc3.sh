#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/../graphenedb_v1_rc3_productization_pack.zip}"
PARENT="$(dirname "$ROOT")"
NAME="$(basename "$ROOT")"
rm -f "$OUT"
(cd "$PARENT" && zip -qr "$OUT" "$NAME" \
  -x "$NAME/build-*/*" \
  -x "$NAME/.git/*" \
  -x "$NAME/*.tmp" \
  -x "$NAME/**/.DS_Store")
echo "Packaged: $OUT"
