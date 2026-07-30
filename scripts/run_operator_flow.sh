#!/usr/bin/env bash
set -euo pipefail

CLI="${CLI:-./build-release/graphenedb_cli}"
WORKDIR="${WORKDIR:-$(mktemp -d "${TMPDIR:-/tmp}/graphenedb_operator_flow.XXXXXX")}"

if [[ ! -x "$CLI" ]]; then
  echo "graphenedb_cli not found or not executable at $CLI" >&2
  exit 2
fi

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

DB="$WORKDIR/db"
BACKUP="$WORKDIR/backup"
TSV="$WORKDIR/extraction.tsv"
SIG=$(( (1 << 2) | (1 << 18) ))

cat > "$TSV" <<EOF
root-1	Checkout root cause: stale edge cache after migration	0.90,0.10,0.05,0.01	$SIG	9001	root
symptom-1	Checkout timeout symptom observed by customers	0.10,0.90,0.05,0.01	$SIG	9001	symptom
impact-1	Payment failures impacted conversion	0.10,0.20,0.90,0.01	$SIG	9001	impact
EOF

IMPORT_OUT="$("$CLI" extract-tsv "$DB" 4 operator-pack "$TSV" --wal-rotate-bytes 2048)"
grep -q "inserted_nodes=3" <<<"$IMPORT_OUT"

"$CLI" inspect "$DB" 4 --vector-index kdtree --json |
  python -c 'import json,sys; d=json.load(sys.stdin); assert d["nodes_visible"] == 3; assert d["edges_visible"] == 2; assert d["vector_index"] == "kdtree"'

"$CLI" validate "$DB" 4 --json |
  python -c 'import json,sys; d=json.load(sys.stdin); assert d["validation"] == "ok"'

"$CLI" compact "$DB" 4 --json |
  python -c 'import json,sys; d=json.load(sys.stdin); assert d["compacted"] is True'

"$CLI" inspect "$DB" 4 --json |
  python -c 'import json,sys; d=json.load(sys.stdin); assert d["wal_bytes"] == 0'

"$CLI" backup "$DB" 4 "$BACKUP" --json |
  python -c 'import json,sys; d=json.load(sys.stdin); assert d["backup_verified"] is True'

SEARCH_OUT="$("$CLI" search "$DB" 4 "0.10,0.90,0.05,0.01" "$SIG")"
grep -q "target=" <<<"$SEARCH_OUT"

echo "operator_flow_passed=true"
echo "db=$DB"
echo "backup=$BACKUP"
