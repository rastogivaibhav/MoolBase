#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import pathlib
import re
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

def fail(message: str) -> None:
    raise SystemExit(f"distribution_policy_failure: {message}")

def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def validate_repository() -> dict[str, object]:
    license_path = ROOT / "LICENSE"
    notice_path = ROOT / "NOTICE"
    third_party_path = ROOT / "THIRD_PARTY_NOTICES.md"
    dco_path = ROOT / "DCO-1.1.txt"
    contributing_path = ROOT / "CONTRIBUTING.md"
    security_path = ROOT / "SECURITY.md"

    for path in [
        license_path, notice_path, third_party_path, dco_path,
        contributing_path, security_path
    ]:
        if not path.is_file():
            fail(f"missing required file: {path.name}")

    license_text = license_path.read_text(encoding="utf-8")
    required_license_markers = [
        "Apache License",
        "Version 2.0, January 2004",
        "END OF TERMS AND CONDITIONS",
        "Copyright [yyyy] [name of copyright owner]",
        "http://www.apache.org/licenses/LICENSE-2.0",
    ]
    for marker in required_license_markers:
        if marker not in license_text:
            fail(f"LICENSE missing canonical marker: {marker}")
    if "Copyright 2026 Vaibhav Rastogi" in license_text:
        fail("LICENSE contains project-specific copyright; keep attribution in NOTICE")

    notice = notice_path.read_text(encoding="utf-8")
    if "Copyright 2026 Vaibhav Rastogi" not in notice:
        fail("NOTICE missing project copyright attribution")

    third_party = third_party_path.read_text(encoding="utf-8")
    if "FAISS" not in third_party or "not bundled" not in third_party:
        fail("third-party notice must disclose optional FAISS boundary")

    contributing = contributing_path.read_text(encoding="utf-8")
    if "Apache License 2.0" not in contributing or "git commit -s" not in contributing:
        fail("CONTRIBUTING must state Apache-2.0 and DCO sign-off")

    security = security_path.read_text(encoding="utf-8")
    for marker in ["SHA-256", "build-provenance attestation", "SBOM"]:
        if marker not in security:
            fail(f"SECURITY missing distribution marker: {marker}")

    return {
        "license": "Apache-2.0",
        "license_sha256": sha256(license_path),
        "notice_sha256": sha256(notice_path),
        "third_party_notices_sha256": sha256(third_party_path),
        "dco_sha256": sha256(dco_path),
    }

def validate_artifact(package: pathlib.Path, manifest_path: pathlib.Path) -> dict[str, object]:
    if not package.is_file():
        fail(f"package not found: {package}")
    if not manifest_path.is_file():
        fail(f"manifest not found: {manifest_path}")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 2:
        fail("release manifest schema_version must be 2")
    if manifest.get("license") != "Apache-2.0":
        fail("release manifest license must be Apache-2.0")
    source_commit = str(manifest.get("source_commit", ""))
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        fail("release manifest must contain an exact 40-character source commit")
    if manifest.get("package_sha256") != sha256(package):
        fail("package SHA-256 does not match manifest")
    if manifest.get("version") in {None, "", "0.5.0"}:
        fail("invalid or stale public release version")
    if manifest.get("cmake_package_version") in {None, ""}:
        fail("missing CMake package version")

    required_names = {"LICENSE", "NOTICE", "THIRD_PARTY_NOTICES.md"}
    file_names = {pathlib.PurePosixPath(item["path"]).name for item in manifest.get("files", [])}
    missing = required_names - file_names
    if missing:
        fail(f"manifest missing installed legal files: {sorted(missing)}")

    if package.suffix == ".zip":
        with zipfile.ZipFile(package) as archive:
            archive_names = {pathlib.PurePosixPath(name).name for name in archive.namelist()}
        missing_archive = required_names - archive_names
        if missing_archive:
            fail(f"package archive missing legal files: {sorted(missing_archive)}")

    return {
        "package": package.name,
        "package_sha256": sha256(package),
        "version": manifest["version"],
        "cmake_package_version": manifest["cmake_package_version"],
        "source_commit": source_commit,
        "license": manifest["license"],
    }

def main() -> int:
    repo = validate_repository()
    result: dict[str, object] = {"repository": repo}

    if len(sys.argv) == 3:
        result["artifact"] = validate_artifact(
            pathlib.Path(sys.argv[1]).resolve(),
            pathlib.Path(sys.argv[2]).resolve(),
        )
    elif len(sys.argv) != 1:
        raise SystemExit(
            "usage: validate_distribution_policy.py [package manifest.json]"
        )

    print(json.dumps(result, indent=2, sort_keys=True))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
