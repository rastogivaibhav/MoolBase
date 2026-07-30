#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-rc-bundle-ga}"
GA_REPORT_DIR="${GA_REPORT_DIR:-reports/ga-readiness}"
EVIDENCE_DIR="${EVIDENCE_DIR:-reports/ga-evidence}"
PACKAGE_OUT="${PACKAGE_OUT:-$ROOT/graphenedb-install-package.zip}"
CONFIG="${CONFIG:-Release}"
PROFILE_LABEL="${PROFILE_LABEL:-release-candidate-smoke}"
C_TEST_EXCLUDE="${C_TEST_EXCLUDE:-graphenedb_rc_(crash|fuzz|kosh_adapter|stress|1m_storage|soak)_tests}"
FOCUSED_REGEX="${FOCUSED_REGEX:-graphenedb_(acid_lattice|lattice)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests}"
VECTOR_INDEX_RECALL_NODES="${VECTOR_INDEX_RECALL_NODES:-1000}"
VECTOR_INDEX_RECALL_QUERIES="${VECTOR_INDEX_RECALL_QUERIES:-20}"
VECTOR_INDEX_RECALL_DIM="${VECTOR_INDEX_RECALL_DIM:-16}"
VECTOR_INDEX_RECALL_K="${VECTOR_INDEX_RECALL_K:-5}"
VECTOR_INDEX_RECALL_KIND="${VECTOR_INDEX_RECALL_KIND:-auto}"
VECTOR_INDEX_RECALL_MIN="${VECTOR_INDEX_RECALL_MIN:-0.999}"
EXTRACTION_DOCS="${EXTRACTION_DOCS:-3}"
EXTRACTION_NODES_PER_DOC="${EXTRACTION_NODES_PER_DOC:-5}"
EXTRACTION_QUERIES="${EXTRACTION_QUERIES:-2}"
EXTRACTION_VECTOR_INDEX="${EXTRACTION_VECTOR_INDEX:-auto}"
STORAGE_NODES="${STORAGE_NODES:-500}"
STORAGE_QUERIES="${STORAGE_QUERIES:-5}"
DIM="${DIM:-16}"
STORAGE_VECTOR_INDEX="${STORAGE_VECTOR_INDEX:-auto}"
GRAPHENEDB_USE_FAISS="${GRAPHENEDB_USE_FAISS:-0}"

ga_output="$(BUILD_DIR="$BUILD_DIR" REPORT_DIR="$GA_REPORT_DIR" CONFIG="$CONFIG" PROFILE_LABEL="$PROFILE_LABEL" SKIP_PACKAGE=1 \
  CTEST_EXCLUDE="$C_TEST_EXCLUDE" FOCUSED_REGEX="$FOCUSED_REGEX" \
  VECTOR_INDEX_RECALL_NODES="$VECTOR_INDEX_RECALL_NODES" VECTOR_INDEX_RECALL_QUERIES="$VECTOR_INDEX_RECALL_QUERIES" \
  VECTOR_INDEX_RECALL_DIM="$VECTOR_INDEX_RECALL_DIM" VECTOR_INDEX_RECALL_K="$VECTOR_INDEX_RECALL_K" \
  VECTOR_INDEX_RECALL_KIND="$VECTOR_INDEX_RECALL_KIND" VECTOR_INDEX_RECALL_MIN="$VECTOR_INDEX_RECALL_MIN" \
  EXTRACTION_DOCS="$EXTRACTION_DOCS" EXTRACTION_NODES_PER_DOC="$EXTRACTION_NODES_PER_DOC" EXTRACTION_QUERIES="$EXTRACTION_QUERIES" EXTRACTION_VECTOR_INDEX="$EXTRACTION_VECTOR_INDEX" \
  STORAGE_NODES="$STORAGE_NODES" STORAGE_QUERIES="$STORAGE_QUERIES" DIM="$DIM" STORAGE_VECTOR_INDEX="$STORAGE_VECTOR_INDEX" GRAPHENEDB_USE_FAISS="$GRAPHENEDB_USE_FAISS" \
  "$ROOT/scripts/run_ga_readiness.sh")"
ga_summary="$(printf '%s\n' "$ga_output" | tail -n 1)"
ga_run_dir="$(dirname "$ga_summary")"

package_artifact="$("$ROOT/scripts/package_release_install.sh" "$PACKAGE_OUT" | tail -n 1)"

evidence_output="$(OUT_DIR="$EVIDENCE_DIR" CONFIG="release-candidate" GA_READINESS_DIR="$ga_run_dir" PACKAGE_PATH="$package_artifact" ARCHIVE=1 \
  "$ROOT/scripts/collect_ga_evidence.sh")"

status_report="$("$ROOT/scripts/write_ga_status_report.sh" | tail -n 1)"
status_report_path="$ROOT/reports/GA_STATUS_REPORT.md"

evidence_bundle=""
evidence_archive=""
while IFS= read -r line; do
  case "$line" in
    evidence_bundle=*) evidence_bundle="${line#evidence_bundle=}" ;;
    evidence_archive=*) evidence_archive="${line#evidence_archive=}" ;;
  esac
done <<EOF
$evidence_output
EOF

bundle_meta="$ROOT/reports/RELEASE_CANDIDATE_BUNDLE_META.json"

python3 - "$bundle_meta" "$status_report_path" "$ga_run_dir" "$package_artifact" "$evidence_bundle" "$evidence_archive" "$PROFILE_LABEL" "$VECTOR_INDEX_RECALL_KIND" "$EXTRACTION_VECTOR_INDEX" "$STORAGE_VECTOR_INDEX" "$GRAPHENEDB_USE_FAISS" <<'PY'
import datetime as dt
import json
import pathlib
import re
import sys

out, status_report, ga_run_dir, package_artifact, evidence_bundle, evidence_archive, profile_label, recall_kind, extraction_index, storage_index, use_faiss = sys.argv[1:]
summary = pathlib.Path(ga_run_dir) / "GA_READINESS_SUMMARY.md"
text = summary.read_text(encoding="utf-8", errors="ignore") if summary.exists() else ""

def field(key: str) -> str:
    m = re.search(r"^- " + re.escape(key) + r":\s*(.+)$", text, re.MULTILINE)
    return m.group(1).strip() if m else ""

payload = {
    "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
    "profile_label": profile_label,
    "status_report": status_report,
    "ga_summary": str(summary),
    "evidence_bundle": evidence_bundle,
    "evidence_archive": evidence_archive,
    "package_path": package_artifact,
    "package_sha256": f"{package_artifact}.sha256",
    "package_manifest": f"{package_artifact}.manifest.json",
    "requested": {
        "vector_index_recall_kind": recall_kind,
        "extraction_vector_index": extraction_index,
        "storage_vector_index": storage_index,
        "use_faiss": use_faiss in {"1", "true", "True", "TRUE"},
    },
    "resolved": {
        "vector_index_recall": field("vector_index_recall"),
        "vector_index_recall_requested": field("vector_index_recall_requested"),
        "vector_index_recall_mean_recall_at_k": field("vector_index_recall_mean_recall_at_k"),
        "extraction_vector_index": field("extraction_vector_index"),
        "extraction_vector_index_requested": field("extraction_vector_index_requested"),
        "storage_vector_index": field("storage_vector_index"),
        "storage_vector_index_requested": field("storage_vector_index_requested"),
        "graphenedb_use_faiss": field("graphenedb_use_faiss"),
    },
}
pathlib.Path(out).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

python3 "$ROOT/scripts/validate_release_candidate_bundle_meta.py" "$bundle_meta"

echo "release_candidate_bundle=true"
echo "ga_summary=$ga_summary"
echo "package=$package_artifact"
echo "bundle_meta=$bundle_meta"
echo "status_report=$status_report_path"
echo "$status_report"
printf '%s\n' "$evidence_output" | grep '^evidence_bundle='
printf '%s\n' "$evidence_output" | grep '^evidence_archive='
