#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_collect_ga_evidence.py <collector.py>", file=sys.stderr)
        return 2

    collector = Path(sys.argv[1]).resolve()
    out_dir = Path(tempfile.mkdtemp(prefix="graphenedb_evidence_"))
    try:
        proc = subprocess.run(
            [sys.executable, str(collector), "--out-dir", str(out_dir), "--archive"],
            check=False,
            text=True,
            capture_output=True,
        )
        if proc.returncode != 0:
            print(proc.stdout, file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
            print("collector failed", file=sys.stderr)
            return 1

        bundle_dir = None
        archive_path = None
        for line in (proc.stdout + proc.stderr).splitlines():
            if line.startswith("evidence_bundle="):
                bundle_dir = Path(line.split("=", 1)[1].strip())
            if line.startswith("evidence_archive="):
                archive_path = Path(line.split("=", 1)[1].strip())

        if bundle_dir is None or archive_path is None:
            print(proc.stdout, file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
            print("collector did not report bundle and archive paths", file=sys.stderr)
            return 1

        summary = bundle_dir / "EVIDENCE_SUMMARY.md"
        manifest = bundle_dir / "EVIDENCE_MANIFEST.json"
        if not summary.exists() or not manifest.exists():
            print("bundle missing summary or manifest", file=sys.stderr)
            return 1
        summary_text = summary.read_text(encoding="utf-8")
        if "package_path:" not in summary_text or "package_sha256:" not in summary_text or "package_manifest:" not in summary_text:
            print(summary_text, file=sys.stderr)
            print("summary missing package metadata", file=sys.stderr)
            return 1

        manifest_data = json.loads(manifest.read_text(encoding="utf-8"))
        if manifest_data.get("missing_required"):
            print(json.dumps(manifest_data, indent=2), file=sys.stderr)
            print("collector reported missing required evidence", file=sys.stderr)
            return 1

        if not archive_path.exists():
            print(f"archive missing: {archive_path}", file=sys.stderr)
            return 1

        print("collect_ga_evidence_passed=true")
        return 0
    finally:
        # Leave the archived bundle in place; the temp tree can be removed.
        if out_dir.exists():
            for child in sorted(out_dir.glob("*"), reverse=True):
                if child.is_dir():
                    subprocess.run(["cmd", "/c", "rmdir", "/s", "/q", str(child)], capture_output=True, text=True)
                else:
                    child.unlink(missing_ok=True)
            out_dir.rmdir()


if __name__ == "__main__":
    raise SystemExit(main())
