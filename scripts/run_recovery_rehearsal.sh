#!/usr/bin/env bash
set -euo pipefail

CLI="${CLI:-./build-release/graphenedb_cli}"
WORKDIR="${WORKDIR:-$(mktemp -d "${TMPDIR:-/tmp}/graphenedb_recovery_rehearsal.XXXXXX")}"
OUT="${OUT:-reports/RECOVERY_REHEARSAL_OUTPUT.txt}"

if [[ ! -x "$CLI" ]]; then
  echo "graphenedb_cli not found or not executable at $CLI" >&2
  exit 2
fi

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR" "$(dirname "$OUT")"

DB="$WORKDIR/source-db"
BACKUP="$WORKDIR/restored-db"
TSV="$WORKDIR/incident.tsv"
SIG=$(( (1 << 3) | (1 << 19) ))

cat > "$TSV" <<EOF
root-restore	Recovery rehearsal root cause: stale cache after deploy	0.90,0.05,0.03,0.02	$SIG	9101	root
symptom-restore	Recovery rehearsal symptom: checkout timeout	0.05,0.90,0.03,0.02	$SIG	9101	symptom
impact-restore	Recovery rehearsal impact: conversion loss	0.05,0.10,0.90,0.02	$SIG	9101	impact
EOF

: > "$OUT"
log_line() {
  echo "$1" | tee -a "$OUT"
}

log_line "recovery_rehearsal=true"
log_line "source_db=$DB"
log_line "restored_db=$BACKUP"

IMPORT_OUT="$("$CLI" extract-tsv "$DB" 4 recovery-pack "$TSV" --wal-rotate-bytes 2048)"
grep -q "inserted_nodes=3" <<<"$IMPORT_OUT"
log_line "import=pass"

"$CLI" validate "$DB" 4 --json |
  python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["validation"] == "ok"'
log_line "source_validate=pass"

"$CLI" compact "$DB" 4 --json |
  python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["compacted"] is True'
log_line "source_compact=pass"

"$CLI" backup "$DB" 4 "$BACKUP" --json |
  python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["backup_verified"] is True'
log_line "backup_verified=pass"

"$CLI" inspect "$BACKUP" 4 --json |
  python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["nodes_visible"] == 3; assert d["edges_visible"] == 2; print("restored_nodes={}\nrestored_edges={}".format(d["nodes_visible"], d["edges_visible"]))' |
  tee -a "$OUT"

"$CLI" validate "$BACKUP" 4 --json |
  python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["validation"] == "ok"'
log_line "restored_validate=pass"

SEARCH_OUT="$("$CLI" search "$BACKUP" 4 "0.05,0.90,0.03,0.02" "$SIG")"
grep -q "target=" <<<"$SEARCH_OUT"
log_line "restored_search=pass"

NEIGHBORS_OUT="$("$CLI" neighbors "$BACKUP" 4 0 2)"
grep -q "1" <<<"$NEIGHBORS_OUT"
log_line "restored_neighbors=pass"

log_line "recovery_rehearsal_passed=true"
echo "recovery_rehearsal_output=$OUT"
