#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = [
    "README.md",
    "SECURITY.md",
    "LICENSE",
    "CHANGELOG.md",
    "docs/GA_READINESS_SCORECARD.md",
    "docs/NEXT_GA_EXECUTION_PLAN.md",
    "docs/GA_READINESS_VERIFICATION.md",
    "docs/GRAPHENE_LATTICE_MODEL.md",
    "docs/LATTICE_RETRIEVAL.md",
    "docs/EXTRACTION_INGESTION.md",
    "docs/PACKAGING_DISTRIBUTION.md",
    "docs/OPERATIONAL_RECOVERY.md",
    "docs/STORAGE_FORMAT.md",
    "docs/SAFETY_AND_LIMITATIONS.md",
    "docs/PLATFORM_SUPPORT.md",
    "docs/C_API.md",
    "docs/CI_RELEASE_AUTOMATION.md",
    "docs/V1_RC_ACCEPTANCE_REPORT.md",
    "docs/RELEASE_CHECKLIST.md",
]
REPORTS = [
    "reports/RC5_ACID_REPORT.md",
    "reports/RC5_CRASH_MATRIX.md",
    "reports/RC5_STORAGE_RETRIEVAL_PERF.md",
    "reports/EXTRACTION_INGEST_PERF.md",
    "reports/VECTOR_BASELINE_COMPARISON.md",
    "reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt",
    "reports/VECTOR_INDEX_RECALL.md",
    "reports/VECTOR_INDEX_RECALL_OUTPUT.txt",
    "reports/RECOVERY_REHEARSAL.md",
    "reports/RECOVERY_REHEARSAL_OUTPUT.txt",
    "reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt",
    "reports/KOSH_ADAPTER_GATE.md",
    "reports/RELEASE_CANDIDATE_BUNDLE.md",
    "reports/GA_STATUS_REPORT.md",
    "reports/GA_PROGRESS_RC_BUNDLE.md",
    "reports/GA_PROGRESS_GA_HARNESS.md",
    "reports/GA_PROGRESS_FILESYSTEM_FAILURES.md",
    "reports/GA_PROGRESS_EXTRACTION_CONTRACT.md",
    "reports/RELEASE_CANDIDATE_BUNDLE_META.json",
]


def latest_dir(base: Path, prefer_passing_summary: str | None = None) -> Path | None:
    if not base.is_dir():
        return None
    dirs = sorted((p for p in base.iterdir() if p.is_dir()), key=lambda p: p.name, reverse=True)
    if prefer_passing_summary:
        for p in dirs:
            summary = p / prefer_passing_summary
            if summary.exists():
                lines = [ln for ln in summary.read_text(encoding="utf-8-sig", errors="replace").splitlines() if ln.strip()]
                if lines and lines[-1] == "PASS":
                    return p
    return dirs[0] if dirs else None


def copy_any(src: Path, dst: Path) -> None:
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="reports/ga-evidence")
    ap.add_argument("--config", default="developer-preview")
    ap.add_argument("--ga-readiness-dir", default="")
    ap.add_argument("--package-path", default="")
    ap.add_argument("--archive", action="store_true")
    args = ap.parse_args()

    out_root = ROOT / args.out_dir
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S")
    bundle_dir = out_root / stamp
    bundle_dir.mkdir(parents=True, exist_ok=True)

    ga_dir = Path(args.ga_readiness_dir) if args.ga_readiness_dir else latest_dir(ROOT / "reports" / "ga-readiness", "GA_READINESS_SUMMARY.md")
    attempt_dir = latest_dir(ROOT / "reports" / "ga-readiness")
    enterprise_dir = latest_dir(ROOT / "reports" / "enterprise-ga")
    preview_dir = latest_dir(ROOT / "reports" / "preview-hardware", "PREVIEW_HARDWARE_SUMMARY.md")

    package_path: Path | None = Path(args.package_path) if args.package_path else ROOT / "graphenedb-install-package.zip"
    if package_path is not None and not package_path.exists():
        package_path = None

    records: list[dict[str, object]] = []

    for rel in DOCS:
        src = ROOT / rel
        rec = {"kind": "doc", "source": str(src), "destination": str(bundle_dir / rel), "required": True, "present": src.exists()}
        if src.exists():
            copy_any(src, bundle_dir / rel)
            if src.is_file():
                rec["sha256"] = sha256_of(bundle_dir / rel)
                rec["bytes"] = (bundle_dir / rel).stat().st_size
        records.append(rec)

    for rel in REPORTS:
        src = ROOT / rel
        rec = {"kind": "report", "source": str(src), "destination": str(bundle_dir / rel), "required": False, "present": src.exists()}
        if src.exists():
            copy_any(src, bundle_dir / rel)
            if src.is_file():
                rec["sha256"] = sha256_of(bundle_dir / rel)
                rec["bytes"] = (bundle_dir / rel).stat().st_size
        records.append(rec)

    if ga_dir:
        dst = bundle_dir / "ga-readiness" / "latest"
        copy_any(ga_dir, dst)
        records.append({"kind": "ga_readiness_run", "source": str(ga_dir), "destination": str(dst), "required": False, "present": True})
    if attempt_dir:
        dst = bundle_dir / "ga-readiness" / "latest-attempt"
        copy_any(attempt_dir, dst)
        records.append({"kind": "ga_readiness_attempt_run", "source": str(attempt_dir), "destination": str(dst), "required": False, "present": True})
    if enterprise_dir:
        dst = bundle_dir / "enterprise-ga" / "latest"
        copy_any(enterprise_dir, dst)
        records.append({"kind": "enterprise_ga_run", "source": str(enterprise_dir), "destination": str(dst), "required": False, "present": True})
    if preview_dir:
        dst = bundle_dir / "preview-hardware" / "latest"
        copy_any(preview_dir, dst)
        records.append({"kind": "preview_hardware_run", "source": str(preview_dir), "destination": str(dst), "required": False, "present": True})

    if package_path is not None:
        dst = bundle_dir / "package" / package_path.name
        copy_any(package_path, dst)
        records.append({"kind": "package", "source": str(package_path), "destination": str(dst), "required": False, "present": True, "sha256": sha256_of(dst), "bytes": dst.stat().st_size})
        for suffix, kind in [(".sha256", "package_sha256"), (".manifest.json", "package_manifest")]:
            src = Path(str(package_path) + suffix)
            dst = bundle_dir / "package" / (package_path.name + suffix)
            rec = {"kind": kind, "source": str(src), "destination": str(dst), "required": False, "present": src.exists()}
            if src.exists():
                copy_any(src, dst)
                rec["sha256"] = sha256_of(dst)
                rec["bytes"] = dst.stat().st_size
            records.append(rec)

    git_status = bundle_dir / "git-status.txt"
    git_status.write_text(subprocess.run(["git", "-C", str(ROOT), "status", "--short"], capture_output=True, text=True, check=False).stdout, encoding="utf-8")
    records.append({
        "kind": "git_status",
        "source": "git status --short",
        "destination": str(git_status),
        "required": False,
        "present": True,
        "sha256": sha256_of(git_status),
        "bytes": git_status.stat().st_size,
    })

    missing_required = [r["source"] for r in records if r.get("required") and not r.get("present")]
    missing_optional = [r["source"] for r in records if not r.get("required") and not r.get("present")]
    manifest = {
        "name": "GrapheneDB GA evidence bundle",
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "config": args.config,
        "root": str(ROOT),
        "bundle_dir": str(bundle_dir),
        "ga_readiness_dir": str(ga_dir or ""),
        "package_path": str(package_path or ""),
        "missing_required": missing_required,
        "missing_optional": missing_optional,
        "artifacts": records,
    }
    (bundle_dir / "EVIDENCE_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    summary_lines = [
        "# GrapheneDB GA Evidence Bundle",
        "",
        f"- generated_at_utc: {manifest['generated_at_utc']}",
        f"- config: {args.config}",
        f"- bundle_dir: {bundle_dir}",
        f"- ga_readiness_dir: {ga_dir or ''}",
        f"- package_path: {package_path or ''}",
    ]
    if package_path is not None:
        summary_lines.append(f"- package_sha256: {package_path}.sha256")
        summary_lines.append(f"- package_manifest: {package_path}.manifest.json")
    summary_lines += [
        f"- missing_required_count: {len(missing_required)}",
        f"- missing_optional_count: {len(missing_optional)}",
        "",
        "See `EVIDENCE_MANIFEST.json` for the artifact inventory and hashes.",
        "",
    ]
    (bundle_dir / "EVIDENCE_SUMMARY.md").write_text("\n".join(summary_lines), encoding="utf-8")

    if missing_required:
        print("missing required evidence: " + ", ".join(missing_required), file=sys.stderr)
        return 2

    archive_path = None
    if args.archive:
        archive_path = bundle_dir.with_suffix(".zip")
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for path in bundle_dir.rglob("*"):
                if path.is_file():
                    zf.write(path, arcname=str(path.relative_to(out_root)))
        print(f"evidence_archive={archive_path}")

    print(f"evidence_bundle={bundle_dir}")
    print(f"evidence_manifest={bundle_dir / 'EVIDENCE_MANIFEST.json'}")
    print(f"missing_optional_count={len(missing_optional)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
