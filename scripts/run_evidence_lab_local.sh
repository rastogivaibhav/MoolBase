#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GATEWAY="$ROOT/apps/evidence_lab/gateway"
SAMPLES="$ROOT/apps/evidence_lab/samples"

cd "$GATEWAY"
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -e '.[test]'
pytest

export EVIDENCE_LAB_BACKEND="${EVIDENCE_LAB_BACKEND:-recorded}"
export EVIDENCE_LAB_SAMPLES_DIR="$SAMPLES"
export EVIDENCE_LAB_ALLOWED_ORIGINS="http://127.0.0.1:8088,http://localhost:8088"

printf '\nGateway: http://127.0.0.1:8080/docs\n'
printf 'Site: run in a second terminal:\n'
printf '  python3 -m http.server 8088 -d %q\n\n' "$ROOT/apps/evidence_lab/site"
exec uvicorn evidence_lab.main:app --host 127.0.0.1 --port 8080
