#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-reports/ga-evidence}"
CONFIG="${CONFIG:-developer-preview}"
GA_READINESS_DIR="${GA_READINESS_DIR:-}"
PACKAGE_PATH="${PACKAGE_PATH:-}"
ARCHIVE="${ARCHIVE:-0}"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
BUNDLE_DIR="$ROOT/$OUT_DIR/$STAMP"
MANIFEST="$BUNDLE_DIR/EVIDENCE_MANIFEST.json"
SUMMARY="$BUNDLE_DIR/EVIDENCE_SUMMARY.md"
mkdir -p "$BUNDLE_DIR"

repo_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$ROOT/$1" ;;
  esac
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

latest_ga_run() {
  if [[ -d "$ROOT/reports/ga-readiness" ]]; then
    while IFS= read -r dir; do
      if [[ -f "$dir/GA_READINESS_SUMMARY.md" ]] && [[ "$(grep -v '^$' "$dir/GA_READINESS_SUMMARY.md" | tail -n 1)" == "PASS" ]]; then
        echo "$dir"
        return 0
      fi
    done < <(find "$ROOT/reports/ga-readiness" -mindepth 1 -maxdepth 1 -type d | sort -r)
    find "$ROOT/reports/ga-readiness" -mindepth 1 -maxdepth 1 -type d | sort -r | head -n 1
  fi
}

latest_ga_attempt_run() {
  if [[ -d "$ROOT/reports/ga-readiness" ]]; then
    find "$ROOT/reports/ga-readiness" -mindepth 1 -maxdepth 1 -type d | sort -r | head -n 1
  fi
}

latest_enterprise_run() {
  if [[ -d "$ROOT/reports/enterprise-ga" ]]; then
    find "$ROOT/reports/enterprise-ga" -mindepth 1 -maxdepth 1 -type d | sort -r | head -n 1
  fi
}

latest_preview_profile_run() {
  if [[ -d "$ROOT/reports/preview-hardware" ]]; then
    while IFS= read -r dir; do
      if [[ -f "$dir/PREVIEW_HARDWARE_SUMMARY.md" ]] && [[ "$(grep -v '^$' "$dir/PREVIEW_HARDWARE_SUMMARY.md" | tail -n 1)" == "PASS" ]]; then
        echo "$dir"
        return 0
      fi
    done < <(find "$ROOT/reports/preview-hardware" -mindepth 1 -maxdepth 1 -type d | sort -r)
    find "$ROOT/reports/preview-hardware" -mindepth 1 -maxdepth 1 -type d | sort -r | head -n 1
  fi
}

if [[ -z "$GA_READINESS_DIR" ]]; then
  GA_READINESS_DIR="$(latest_ga_run || true)"
fi
if [[ -z "$PACKAGE_PATH" && -f "$ROOT/graphenedb-install-package.zip" ]]; then
  PACKAGE_PATH="$ROOT/graphenedb-install-package.zip"
fi
GA_ATTEMPT_DIR="$(latest_ga_attempt_run || true)"
ENTERPRISE_RUN_DIR="$(latest_enterprise_run || true)"
PREVIEW_PROFILE_RUN_DIR="$(latest_preview_profile_run || true)"

RECORDS_JSON="$BUNDLE_DIR/.records.jsonl"
: > "$RECORDS_JSON"

copy_if_exists() {
  local source="$1"
  local dest_rel="$2"
  local kind="$3"
  local required="$4"
  local src
  src="$(repo_path "$source")"
  local dest="$BUNDLE_DIR/$dest_rel"
  local present="false"
  local sha=""
  local bytes=""
  if [[ -e "$src" ]]; then
    mkdir -p "$(dirname "$dest")"
    cp -R "$src" "$dest"
    present="true"
    if [[ -f "$dest" ]]; then
      sha="$(sha256_file "$dest")"
      bytes="$(wc -c < "$dest" | tr -d ' ')"
    fi
  fi
  python3 - "$kind" "$src" "$dest" "$required" "$present" "$sha" "$bytes" >> "$RECORDS_JSON" <<'PY'
import json, sys
kind, source, dest, required, present, sha, bytes_ = sys.argv[1:]
obj = {
    "kind": kind,
    "source": source,
    "destination": dest,
    "required": required == "true",
    "present": present == "true",
}
if sha:
    obj["sha256"] = sha
if bytes_:
    obj["bytes"] = int(bytes_)
print(json.dumps(obj, sort_keys=False))
PY
}

docs=(
  README.md
  SECURITY.md
  LICENSE
  CHANGELOG.md
  docs/GA_READINESS_SCORECARD.md
  docs/NEXT_GA_EXECUTION_PLAN.md
  docs/GA_READINESS_VERIFICATION.md
  docs/GRAPHENE_LATTICE_MODEL.md
  docs/LATTICE_RETRIEVAL.md
  docs/EXTRACTION_INGESTION.md
  docs/PACKAGING_DISTRIBUTION.md
  docs/OPERATIONAL_RECOVERY.md
  docs/STORAGE_FORMAT.md
  docs/SAFETY_AND_LIMITATIONS.md
  docs/PLATFORM_SUPPORT.md
  docs/C_API.md
  docs/CI_RELEASE_AUTOMATION.md
  docs/V1_RC_ACCEPTANCE_REPORT.md
  docs/RELEASE_CHECKLIST.md
)

for doc in "${docs[@]}"; do
  copy_if_exists "$doc" "$doc" doc true
done

reports=(
  reports/RC5_ACID_REPORT.md
  reports/RC5_CRASH_MATRIX.md
  reports/RC5_STORAGE_RETRIEVAL_PERF.md
  reports/EXTRACTION_INGEST_PERF.md
  reports/VECTOR_BASELINE_COMPARISON.md
  reports/VECTOR_BASELINE_COMPARISON_OUTPUT.txt
  reports/VECTOR_INDEX_RECALL.md
  reports/VECTOR_INDEX_RECALL_OUTPUT.txt
  reports/RECOVERY_REHEARSAL.md
  reports/RECOVERY_REHEARSAL_OUTPUT.txt
  reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt
  reports/KOSH_ADAPTER_GATE.md
  reports/RELEASE_CANDIDATE_BUNDLE.md
  reports/GA_STATUS_REPORT.md
  reports/GA_PROGRESS_RC_BUNDLE.md
  reports/GA_PROGRESS_GA_HARNESS.md
  reports/GA_PROGRESS_FILESYSTEM_FAILURES.md
  reports/GA_PROGRESS_EXTRACTION_CONTRACT.md
  reports/RELEASE_CANDIDATE_BUNDLE_META.json
)

for report in "${reports[@]}"; do
  copy_if_exists "$report" "$report" report false
done

if [[ -n "$GA_READINESS_DIR" ]]; then
  copy_if_exists "$GA_READINESS_DIR" ga-readiness/latest ga_readiness_run false
fi

if [[ -n "$GA_ATTEMPT_DIR" ]]; then
  copy_if_exists "$GA_ATTEMPT_DIR" ga-readiness/latest-attempt ga_readiness_attempt_run false
fi

if [[ -n "$ENTERPRISE_RUN_DIR" ]]; then
  copy_if_exists "$ENTERPRISE_RUN_DIR" enterprise-ga/latest enterprise_ga_run false
fi

if [[ -n "$PREVIEW_PROFILE_RUN_DIR" ]]; then
  copy_if_exists "$PREVIEW_PROFILE_RUN_DIR" preview-hardware/latest preview_hardware_run false
fi

if [[ -n "$PACKAGE_PATH" ]]; then
  package_name="$(basename "$PACKAGE_PATH")"
  copy_if_exists "$PACKAGE_PATH" "package/$package_name" package false
  copy_if_exists "$PACKAGE_PATH.sha256" "package/$package_name.sha256" package_sha256 false
  copy_if_exists "$PACKAGE_PATH.manifest.json" "package/$package_name.manifest.json" package_manifest false
fi

git -C "$ROOT" status --short > "$BUNDLE_DIR/git-status.txt" || true
python3 - "$BUNDLE_DIR/git-status.txt" "$(sha256_file "$BUNDLE_DIR/git-status.txt")" "$(wc -c < "$BUNDLE_DIR/git-status.txt" | tr -d ' ')" >> "$RECORDS_JSON" <<'PY'
import json, sys
path, sha, bytes_ = sys.argv[1:]
print(json.dumps({
    "kind": "git_status",
    "source": "git status --short",
    "destination": path,
    "required": False,
    "present": True,
    "sha256": sha,
    "bytes": int(bytes_),
}, sort_keys=False))
PY

python3 - "$RECORDS_JSON" "$MANIFEST" "$CONFIG" "$ROOT" "$BUNDLE_DIR" "$GA_READINESS_DIR" "$PACKAGE_PATH" <<'PY'
import datetime as dt
import json
import pathlib
import sys

records_path, manifest_path, config, root, bundle_dir, ga_dir, package = sys.argv[1:]
records = [json.loads(line) for line in pathlib.Path(records_path).read_text(encoding="utf-8").splitlines() if line.strip()]
missing_required = [r["source"] for r in records if r.get("required") and not r.get("present")]
missing_optional = [r["source"] for r in records if not r.get("required") and not r.get("present")]
manifest = {
    "name": "GrapheneDB GA evidence bundle",
    "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
    "config": config,
    "root": root,
    "bundle_dir": bundle_dir,
    "ga_readiness_dir": ga_dir,
    "package_path": package,
    "missing_required": missing_required,
    "missing_optional": missing_optional,
    "artifacts": records,
}
pathlib.Path(manifest_path).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
if missing_required:
    print("missing required evidence: " + ", ".join(missing_required), file=sys.stderr)
    sys.exit(2)
PY

missing_optional_count="$(python3 - "$MANIFEST" <<'PY'
import json, sys
print(len(json.load(open(sys.argv[1], encoding="utf-8"))["missing_optional"]))
PY
)"

package_sha_line=""
package_manifest_line=""
if [[ -n "$PACKAGE_PATH" ]]; then
  package_sha_line="$PACKAGE_PATH.sha256"
  package_manifest_line="$PACKAGE_PATH.manifest.json"
fi

cat > "$SUMMARY" <<EOF
# GrapheneDB GA Evidence Bundle

- generated_at_utc: $(python3 - "$MANIFEST" <<'PY'
import json, sys
print(json.load(open(sys.argv[1], encoding="utf-8"))["generated_at_utc"])
PY
)
- config: $CONFIG
- bundle_dir: $BUNDLE_DIR
- ga_readiness_dir: $GA_READINESS_DIR
- package_path: $PACKAGE_PATH
EOF

if [[ -n "$PACKAGE_PATH" ]]; then
  cat >> "$SUMMARY" <<EOF
- package_sha256: $package_sha_line
- package_manifest: $package_manifest_line
EOF
fi

cat >> "$SUMMARY" <<EOF
- missing_optional_count: $missing_optional_count

See \`EVIDENCE_MANIFEST.json\` for the artifact inventory and hashes.
EOF

rm -f "$RECORDS_JSON"

if [[ "$ARCHIVE" == "1" ]]; then
  archive="$BUNDLE_DIR.tar.gz"
  tar -czf "$archive" -C "$(dirname "$BUNDLE_DIR")" "$(basename "$BUNDLE_DIR")"
  echo "evidence_archive=$archive"
fi

echo "evidence_bundle=$BUNDLE_DIR"
echo "evidence_manifest=$MANIFEST"
echo "missing_optional_count=$missing_optional_count"

exit 0
