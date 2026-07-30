#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_TAG="${IMAGE_TAG:-graphenedb:launch-candidate}"
REPORT_DIR="${REPORT_DIR:-$ROOT/reports/container-security}"
mkdir -p "$REPORT_DIR"

command -v docker >/dev/null 2>&1 || {
  echo "docker is required for the container security gate" >&2
  exit 2
}

python3 "$ROOT/scripts/validate_docker_security.py" "$ROOT" \
  | tee "$REPORT_DIR/static-validation.json"
docker build --pull -t "$IMAGE_TAG" "$ROOT" \
  | tee "$REPORT_DIR/docker-build.txt"

docker image inspect "$IMAGE_TAG" > "$REPORT_DIR/image-inspect.json"
python3 - "$REPORT_DIR/image-inspect.json" <<'PY'
import json, sys
image = json.load(open(sys.argv[1]))[0]
config = image.get("Config", {})
user = config.get("User", "")
health = config.get("Healthcheck")
assert user and user not in {"0", "root", "0:0"}, f"image runs as unsafe user: {user!r}"
assert health and health.get("Test"), "image has no healthcheck"
print(f"container_runtime_metadata_passed=true user={user}")
PY

if command -v trivy >/dev/null 2>&1; then
  trivy image --exit-code 1 --severity HIGH,CRITICAL --ignore-unfixed \
    --format json --output "$REPORT_DIR/trivy.json" "$IMAGE_TAG"
elif command -v grype >/dev/null 2>&1; then
  grype "$IMAGE_TAG" --fail-on high -o json > "$REPORT_DIR/grype.json"
else
  echo "trivy or grype is required for the launch container CVE gate" >&2
  exit 2
fi

echo "graphenedb_container_security_gate_passed=true"
