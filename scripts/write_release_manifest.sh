#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <package> <install-dir> [manifest-out]" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE="$1"
INSTALL_DIR="$2"
OUT="${3:-$PACKAGE.manifest.json}"

python - "$ROOT" "$PACKAGE" "$INSTALL_DIR" "$OUT" <<'PY'
import datetime
import hashlib
import json
import os
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()
package = pathlib.Path(sys.argv[2])
install = pathlib.Path(sys.argv[3])
out = pathlib.Path(sys.argv[4])
if not package.is_absolute():
    package = root / package
if not install.is_absolute():
    install = root / install
if not out.is_absolute():
    out = root / out

if not package.exists():
    raise SystemExit(f"package not found: {package}")
if not install.exists():
    raise SystemExit(f"install dir not found: {install}")

def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

files = []
for path in sorted(p for p in install.rglob("*") if p.is_file()):
    files.append({
        "path": path.relative_to(install).as_posix(),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    })

package_hash = sha256(package)
manifest = {
    "name": "GrapheneDB",
    "version": "0.5.0",
    "package": package.name,
    "package_size": package.stat().st_size,
    "package_sha256": package_hash,
    "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "install_prefix": install.name,
    "files": files,
}
out.write_text(json.dumps(manifest, indent=2, sort_keys=False) + "\n", encoding="utf-8")
package.with_suffix(package.suffix + ".sha256").write_text(f"{package_hash}  {package.name}\n", encoding="ascii")
print(f"release_manifest={out}")
print(f"package_sha256={package_hash}")
PY
