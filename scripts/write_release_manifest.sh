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
import pathlib
import re
import subprocess
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

cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
match = re.search(
    r"project\s*\(\s*GrapheneDB\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    cmake_text,
    re.IGNORECASE | re.MULTILINE,
)
if not match:
    raise SystemExit("could not derive GrapheneDB version from CMakeLists.txt")
cmake_package_version = match.group(1)

citation_text = (root / "CITATION.cff").read_text(encoding="utf-8")
software_version = None
for line in citation_text.splitlines():
    if line.startswith("version:"):
        software_version = line.split(":", 1)[1].strip().strip('"').strip("'")
        break
if not software_version:
    raise SystemExit("could not derive public software version from CITATION.cff")
if software_version != cmake_package_version and not software_version.startswith(cmake_package_version + "-"):
    raise SystemExit(
        f"public version {software_version!r} is incompatible with "
        f"CMake package version {cmake_package_version!r}"
    )

try:
    source_commit = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        text=True,
    ).strip()
except Exception:
    source_commit = "unknown"

required_legal = ["LICENSE", "NOTICE", "THIRD_PARTY_NOTICES.md"]
for name in required_legal:
    if not (root / name).is_file():
        raise SystemExit(f"required distribution legal file missing: {name}")

files = []
for path in sorted(p for p in install.rglob("*") if p.is_file()):
    files.append({
        "path": path.relative_to(install).as_posix(),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    })

installed_names = {pathlib.PurePosixPath(item["path"]).name for item in files}
for name in required_legal:
    if name not in installed_names:
        raise SystemExit(f"installed package missing required legal file: {name}")

package_hash = sha256(package)
manifest = {
    "schema_version": 2,
    "name": "GrapheneDB",
    "version": software_version,
    "cmake_package_version": cmake_package_version,
    "license": "Apache-2.0",
    "source_commit": source_commit,
    "package": package.name,
    "package_size": package.stat().st_size,
    "package_sha256": package_hash,
    "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "install_prefix": install.name,
    "legal_files": {
        name: sha256(root / name) for name in required_legal
    },
    "files": files,
}
out.write_text(
    json.dumps(manifest, indent=2, sort_keys=False) + "\n",
    encoding="utf-8",
)
package.with_suffix(package.suffix + ".sha256").write_text(
    f"{package_hash}  {package.name}\n",
    encoding="ascii",
)
print(f"release_manifest={out}")
print(f"package_sha256={package_hash}")
print(f"version={software_version}")
print(f"cmake_package_version={cmake_package_version}")
print(f"source_commit={source_commit}")
PY
