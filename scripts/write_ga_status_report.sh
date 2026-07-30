#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/reports/GA_STATUS_REPORT.md}"
SCORECARD="$ROOT/docs/GA_READINESS_SCORECARD.md"
REPORT_LINES=()

add_report_line() {
  REPORT_LINES+=("$1")
  write_report
}

write_report() {
  local tmp="${OUT}.tmp"
  mkdir -p "$(dirname "$OUT")"
  printf '%s\n' "${REPORT_LINES[@]}" > "$tmp"
  mv -f "$tmp" "$OUT"
}

latest_dir() {
  local path="$1"
  if [[ -d "$path" ]]; then
    find "$path" -mindepth 1 -maxdepth 1 -type d | sort -r | head -n 1
  fi
}

latest_passing_ga_dir() {
  if [[ -d "$ROOT/reports/ga-readiness" ]]; then
    while IFS= read -r dir; do
      if [[ -f "$dir/GA_READINESS_SUMMARY.md" ]] && [[ "$(tail -n 1 "$dir/GA_READINESS_SUMMARY.md")" == "PASS" ]]; then
        echo "$dir"
        return 0
      fi
    done < <(find "$ROOT/reports/ga-readiness" -mindepth 1 -maxdepth 1 -type d | sort -r)
  fi
  latest_dir "$ROOT/reports/ga-readiness"
}

latest_passing_preview_dir() {
  if [[ -d "$ROOT/reports/preview-hardware" ]]; then
    while IFS= read -r dir; do
      if [[ -f "$dir/PREVIEW_HARDWARE_SUMMARY.md" ]] && [[ "$(grep -v '^$' "$dir/PREVIEW_HARDWARE_SUMMARY.md" | tail -n 1)" == "PASS" ]]; then
        echo "$dir"
        return 0
      fi
    done < <(find "$ROOT/reports/preview-hardware" -mindepth 1 -maxdepth 1 -type d | sort -r)
  fi
  latest_dir "$ROOT/reports/preview-hardware"
}

latest_complete_evidence_dir() {
  if [[ -d "$ROOT/reports/ga-evidence" ]]; then
    while IFS= read -r dir; do
      if [[ -f "$dir/EVIDENCE_MANIFEST.json" ]]; then
        echo "$dir"
        return 0
      fi
    done < <(find "$ROOT/reports/ga-evidence" -mindepth 1 -maxdepth 1 -type d | sort -r)
  fi
  latest_dir "$ROOT/reports/ga-evidence"
}

section_items() {
  local header="$1"
  awk -v header="$header" '
    $0 == header {in_section=1; next}
    in_section && /^## / {exit}
    !in_section {next}
    /^[0-9]+\./ {sub(/^[0-9]+\.[[:space:]]+/, "", $0); print; next}
    /^- / {sub(/^- /, "", $0); print; next}
  ' "$SCORECARD"
}

json_field() {
  local path="$1"
  local field="$2"
  python3 - "$path" "$field" <<'PY'
import json, pathlib, sys
path, field = sys.argv[1:]
try:
    raw = pathlib.Path(path).read_bytes()
    for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
        try:
            text = raw.decode(encoding)
            break
        except UnicodeError:
            continue
    else:
        text = raw.decode("utf-8", errors="ignore")
    data = json.loads(text)
except Exception:
    print("")
    raise SystemExit(0)
value = data
for part in field.split('.'):
    if isinstance(value, dict):
        value = value.get(part, "")
    else:
        value = ""
        break
if isinstance(value, bool):
    print("true" if value else "false")
else:
    print(value if value is not None else "")
PY
}

metric_value() {
  local path="$1"
  local key="$2"
  python3 - "$path" "$key" <<'PY'
import pathlib
import re
import sys

path, key = sys.argv[1:]
raw = pathlib.Path(path).read_bytes()
for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
    try:
        text = raw.decode(encoding)
        break
    except UnicodeError:
        continue
else:
    text = raw.decode("utf-8", errors="ignore")
match = re.search(r'(?:^|\s)' + re.escape(key) + r'=([^\s]+)', text)
print(match.group(1) if match else "")
PY
}

markdown_field_value() {
  local path="$1"
  local key="$2"
  python3 - "$path" "$key" <<'PY'
import pathlib
import re
import sys

path, key = sys.argv[1:]
raw = pathlib.Path(path).read_bytes()
for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
    try:
        text = raw.decode(encoding)
        break
    except UnicodeError:
        continue
else:
    text = raw.decode("utf-8", errors="ignore")
match = re.search(r'^- ' + re.escape(key) + r':\s*(.+)$', text, re.MULTILINE)
print(match.group(1) if match else "")
PY
}

failed_ctest_tests() {
  local path="$1"
  python3 - "$path" <<'PY'
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
if not path.exists():
    raise SystemExit(0)
raw = path.read_bytes()
for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
    try:
        text = raw.decode(encoding)
        break
    except UnicodeError:
        continue
else:
    text = raw.decode("utf-8", errors="ignore")
seen = set()
for line in text.splitlines():
    m = re.match(r'^\d+:(.+)$', line.strip())
    if not m:
        continue
    test = m.group(1).strip()
    if test and test not in seen:
        seen.add(test)
        print(test)
PY
}

ctest_succeeded() {
  local success_path="$1"
  local failure_path="$2"
  [[ -f "$success_path" ]] || return 1
  [[ -f "$failure_path" ]] || return 0
  [[ "$success_path" -nt "$failure_path" ]]
}

file_exists() {
  local path="$1"
  [[ -f "$path" ]] && echo true || echo false
}

file_contains() {
  local path="$1"
  local needle="$2"
  python3 - "$path" "$needle" <<'PY'
import pathlib
import sys

path, needle = sys.argv[1:]
try:
    raw = pathlib.Path(path).read_bytes()
except FileNotFoundError:
    print("false")
    raise SystemExit(0)
for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
    try:
        text = raw.decode(encoding)
        break
    except UnicodeError:
        continue
else:
    text = raw.decode("utf-8", errors="ignore")
print("true" if needle in text else "false")
PY
}

filter_pending_items() {
  local approved_host="$1"
  local intended_hardware="$2"
  while IFS= read -r item; do
    [[ -z "$item" ]] && continue
    if [[ "$approved_host" == "true" && "$item" == "Run the GA readiness harness on a host that does not block freshly built executables." ]]; then
      continue
    fi
    if [[ "$approved_host" == "true" && "$item" == "Run the full default GA readiness harness on an approved build host without local policy exclusions." ]]; then
      continue
    fi
    if [[ "$intended_hardware" == "true" && "$item" == "Publish preserved benchmark reports for the intended developer-preview hardware profile." ]]; then
      continue
    fi
    echo "$item"
  done
}

latest_ga_dir="$(latest_passing_ga_dir)"
latest_ga_attempt_dir="$(latest_dir "$ROOT/reports/ga-readiness")"
latest_evidence_dir="$(latest_complete_evidence_dir)"
latest_enterprise_dir="$(latest_dir "$ROOT/reports/enterprise-ga")"
latest_preview_profile_dir="$(latest_passing_preview_dir)"
latest_ga_summary="${latest_ga_dir:+$latest_ga_dir/GA_READINESS_SUMMARY.md}"
latest_ga_attempt_summary="${latest_ga_attempt_dir:+$latest_ga_attempt_dir/GA_READINESS_SUMMARY.md}"
latest_evidence_manifest="${latest_evidence_dir:+$latest_evidence_dir/EVIDENCE_MANIFEST.json}"
latest_enterprise_summary="${latest_enterprise_dir:+$latest_enterprise_dir/ENTERPRISE_GA_SUMMARY.md}"
latest_preview_profile_summary="${latest_preview_profile_dir:+$latest_preview_profile_dir/PREVIEW_HARDWARE_SUMMARY.md}"
latest_enterprise_100k_output="${latest_enterprise_dir:+$latest_enterprise_dir/RC_STRESS_100K_OUTPUT.txt}"
latest_enterprise_1m_output="${latest_enterprise_dir:+$latest_enterprise_dir/RC_STRESS_1M_STORAGE_OUTPUT.txt}"
latest_preview_vector_recall_output="${latest_preview_profile_dir:+$latest_preview_profile_dir/VECTOR_INDEX_RECALL_OUTPUT.txt}"
latest_preview_extraction_output="${latest_preview_profile_dir:+$latest_preview_profile_dir/EXTRACTION_INGEST_OUTPUT.txt}"
latest_preview_storage_output="${latest_preview_profile_dir:+$latest_preview_profile_dir/RC5_STORAGE_RETRIEVAL_OUTPUT.txt}"
latest_ga_host_profile="${latest_ga_dir:+$latest_ga_dir/HOST_PROFILE.json}"
latest_preview_host_profile="${latest_preview_profile_dir:+$latest_preview_profile_dir/HOST_PROFILE.json}"
latest_ga_attempt_host_profile="${latest_ga_attempt_dir:+$latest_ga_attempt_dir/HOST_PROFILE.json}"
latest_enterprise_host_profile="${latest_enterprise_dir:+$latest_enterprise_dir/HOST_PROFILE.json}"
ctest_failed_log="$ROOT/build-release/Testing/Temporary/LastTestsFailed.log"
ctest_last_log="$ROOT/build-release/Testing/Temporary/LastTest.log"
approved_host="$([[ -f "$latest_ga_host_profile" ]] && [[ "$(json_field "$latest_ga_host_profile" approved_host)" == "true" ]] && echo true || echo false)"
intended_preview_hardware="$([[ -f "$latest_preview_host_profile" ]] && [[ "$(json_field "$latest_preview_host_profile" intended_hardware)" == "true" ]] && echo true || echo false)"
enterprise_approved_host="$([[ -f "$latest_enterprise_host_profile" ]] && [[ "$(json_field "$latest_enterprise_host_profile" approved_host)" == "true" ]] && echo true || echo false)"
enterprise_100k_dim="$([[ -f "$latest_enterprise_100k_output" ]] && metric_value "$latest_enterprise_100k_output" dim || echo 0)"
enterprise_1m_dim="$([[ -f "$latest_enterprise_1m_output" ]] && metric_value "$latest_enterprise_1m_output" dim || echo 0)"
enterprise_rich_harness_present="$([[ -f "$latest_enterprise_100k_output" && -f "$latest_enterprise_1m_output" ]] && echo true || echo false)"
enterprise_target_dim_ready="$([[ "${enterprise_100k_dim:-0}" -ge 384 && "${enterprise_1m_dim:-0}" -ge 768 ]] && echo true || echo false)"
ga_profile_label="$([[ -f "$latest_ga_host_profile" ]] && json_field "$latest_ga_host_profile" profile_label || true)"
preview_profile_label="$([[ -f "$latest_preview_host_profile" ]] && json_field "$latest_preview_host_profile" profile_label || true)"
ga_attempt_profile_label="$([[ -f "$latest_ga_attempt_host_profile" ]] && json_field "$latest_ga_attempt_host_profile" profile_label || true)"
enterprise_profile_label="$([[ -f "$latest_enterprise_host_profile" ]] && json_field "$latest_enterprise_host_profile" profile_label || true)"

package="$ROOT/graphenedb-install-package.zip"
package_sha="$package.sha256"
package_manifest="$package.manifest.json"
bundle_meta="$ROOT/reports/RELEASE_CANDIDATE_BUNDLE_META.json"
recovery_output="$ROOT/reports/RECOVERY_REHEARSAL_OUTPUT.txt"
kosh_output="$ROOT/reports/RC_REAL_KOSH_ADAPTER_OUTPUT.txt"
vector_recall_output="$ROOT/reports/VECTOR_INDEX_RECALL_OUTPUT.txt"
rc_bundle_report="$ROOT/reports/GA_PROGRESS_RC_BUNDLE.md"
harness_report="$ROOT/reports/GA_PROGRESS_GA_HARNESS.md"
filesystem_report="$ROOT/reports/GA_PROGRESS_FILESYSTEM_FAILURES.md"
extraction_report="$ROOT/reports/GA_PROGRESS_EXTRACTION_CONTRACT.md"

pass_count=0
total_count=13

ga_pass="false"
if [[ -f "$latest_ga_summary" ]] && [[ "$(grep -v '^$' "$latest_ga_summary" | tail -n 1)" == "PASS" ]]; then
  ga_pass="true"
  pass_count=$((pass_count + 1))
fi
[[ -f "$latest_evidence_manifest" ]] && pass_count=$((pass_count + 1))
[[ -f "$package" ]] && pass_count=$((pass_count + 1))
[[ -f "$package_sha" ]] && pass_count=$((pass_count + 1))
[[ -f "$package_manifest" ]] && pass_count=$((pass_count + 1))
[[ -f "$bundle_meta" ]] && pass_count=$((pass_count + 1))
recovery_pass="$(file_contains "$recovery_output" 'recovery_rehearsal_passed=true')"
kosh_pass="$(file_contains "$kosh_output" 'rc_real_kosh_adapter_gate_passed=true')"
vector_pass="$(file_contains "$vector_recall_output" 'mean_recall_at_k=')"
[[ "$recovery_pass" == "true" ]] && pass_count=$((pass_count + 1))
[[ "$kosh_pass" == "true" ]] && pass_count=$((pass_count + 1))
[[ "$vector_pass" == "true" ]] && pass_count=$((pass_count + 1))
[[ -f "$rc_bundle_report" ]] && pass_count=$((pass_count + 1))
[[ -f "$harness_report" ]] && pass_count=$((pass_count + 1))
[[ -f "$filesystem_report" ]] && pass_count=$((pass_count + 1))
[[ -f "$extraction_report" ]] && pass_count=$((pass_count + 1))

add_report_line "# GrapheneDB GA Status Report"
add_report_line ""
add_report_line "- generated_at_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
add_report_line "- local_evidence_passed: $pass_count/$total_count"
add_report_line "- latest_ga_readiness_dir: $latest_ga_dir"
add_report_line "- latest_ga_attempt_dir: $latest_ga_attempt_dir"
add_report_line "- latest_ga_evidence_dir: $latest_evidence_dir"
add_report_line "- latest_enterprise_ga_dir: $latest_enterprise_dir"
add_report_line "- latest_preview_hardware_dir: $latest_preview_profile_dir"
add_report_line ""
should_fix_count=0
must_fix_count=0
mapfile -t should_fix < <(section_items "## Should-fix before public developer preview" | filter_pending_items "$approved_host" "$intended_preview_hardware")
mapfile -t must_fix < <(section_items "## Must-fix before enterprise GA" | filter_pending_items "$approved_host" "$intended_preview_hardware")
should_fix_count="${#should_fix[@]}"
must_fix_count="${#must_fix[@]}"
add_report_line "## Pending Counts"
add_report_line "- public_developer_preview: $should_fix_count"
add_report_line "- enterprise_ga: $must_fix_count"
add_report_line ""
add_report_line "## Locally Evidenced"

status_line() {
  local pass="$1"
  local present="$2"
  local label="$3"
  local path="$4"
  local status="MISSING"
  [[ "$present" == "true" ]] && status="PARTIAL"
  [[ "$pass" == "true" ]] && status="PASS"
  add_report_line "- [$status] $label: \`$path\`"
}

status_line "$ga_pass" "$([[ -f "$latest_ga_summary" ]] && echo true || echo false)" "Latest GA readiness summary" "$latest_ga_summary"
status_line "$([[ -f "$latest_evidence_manifest" ]] && echo true || echo false)" "$([[ -f "$latest_evidence_manifest" ]] && echo true || echo false)" "Latest GA evidence manifest" "$latest_evidence_manifest"
status_line "$([[ -f "$package" ]] && echo true || echo false)" "$([[ -f "$package" ]] && echo true || echo false)" "Install package archive" "$package"
status_line "$([[ -f "$package_sha" ]] && echo true || echo false)" "$([[ -f "$package_sha" ]] && echo true || echo false)" "Install package sha256" "$package_sha"
status_line "$([[ -f "$package_manifest" ]] && echo true || echo false)" "$([[ -f "$package_manifest" ]] && echo true || echo false)" "Install package manifest" "$package_manifest"
status_line "$([[ -f "$bundle_meta" ]] && echo true || echo false)" "$([[ -f "$bundle_meta" ]] && echo true || echo false)" "Release candidate bundle metadata" "$bundle_meta"
status_line "$recovery_pass" "$(file_exists "$recovery_output")" "Recovery rehearsal output" "$recovery_output"
status_line "$kosh_pass" "$(file_exists "$kosh_output")" "Kosh adapter gate output" "$kosh_output"
status_line "$vector_pass" "$(file_exists "$vector_recall_output")" "Vector index recall output" "$vector_recall_output"
status_line "$([[ -f "$rc_bundle_report" ]] && echo true || echo false)" "$([[ -f "$rc_bundle_report" ]] && echo true || echo false)" "RC bundle progress report" "$rc_bundle_report"
status_line "$([[ -f "$harness_report" ]] && echo true || echo false)" "$([[ -f "$harness_report" ]] && echo true || echo false)" "GA harness progress report" "$harness_report"
status_line "$([[ -f "$filesystem_report" ]] && echo true || echo false)" "$([[ -f "$filesystem_report" ]] && echo true || echo false)" "Filesystem failure progress report" "$filesystem_report"
status_line "$([[ -f "$extraction_report" ]] && echo true || echo false)" "$([[ -f "$extraction_report" ]] && echo true || echo false)" "Extraction contract progress report" "$extraction_report"
if [[ -f "$ctest_failed_log" ]]; then
  mapfile -t ctest_blockers < <(failed_ctest_tests "$ctest_failed_log")
  if ctest_succeeded "$ctest_last_log" "$ctest_failed_log"; then
    ctest_blockers=()
  fi
  if [[ "${#ctest_blockers[@]}" -gt 0 ]]; then
    add_report_line ""
    add_report_line "## Local CTest Blockers"
    add_report_line "- latest_failed_log: \`$ctest_failed_log\`"
    for blocker in "${ctest_blockers[@]}"; do
      [[ -n "$blocker" ]] && add_report_line "- $blocker"
    done
  fi
fi

add_report_line ""
add_report_line "## Enterprise Campaign"
enterprise_pass="false"
if [[ -f "$latest_enterprise_summary" ]] && [[ "$(grep -v '^$' "$latest_enterprise_summary" | tail -n 1)" == "PASS" ]]; then
  enterprise_pass="true"
fi
status="MISSING"
[[ -f "$latest_enterprise_summary" ]] && status="PARTIAL"
[[ "$enterprise_pass" == "true" ]] && status="PASS"
add_report_line "- [$status] Latest enterprise campaign summary: \`$latest_enterprise_summary\`"
enterprise_host_status="MISSING"
[[ -f "$latest_enterprise_host_profile" ]] && enterprise_host_status="PARTIAL"
[[ "$enterprise_approved_host" == "true" ]] && enterprise_host_status="PASS"
add_report_line "- [$enterprise_host_status] Host profile: \`$latest_enterprise_host_profile\`"
if [[ -f "$latest_enterprise_host_profile" ]]; then
  add_report_line "- approved_host: $enterprise_approved_host"
  add_report_line "- profile_label: $enterprise_profile_label"
fi
add_report_line "- rich_workload_harness_present: $enterprise_rich_harness_present"
add_report_line "- target_scale_dimensions_ready: $enterprise_target_dim_ready"
if [[ -f "$latest_enterprise_100k_output" ]]; then
  add_report_line "- 100k_dim: $enterprise_100k_dim"
  add_report_line "- 100k_vector_index_requested: $(metric_value "$latest_enterprise_100k_output" vector_index_requested)"
  add_report_line "- 100k_vector_index: $(metric_value "$latest_enterprise_100k_output" vector_index)"
  add_report_line "- 100k_ingest_nodes_per_sec: $(metric_value "$latest_enterprise_100k_output" ingest_nodes_per_sec)"
  add_report_line "- 100k_causal_hit_rate: $(metric_value "$latest_enterprise_100k_output" causal_hit_rate)"
  add_report_line "- 100k_causal_p95_ms: $(metric_value "$latest_enterprise_100k_output" causal_p95_ms)"
  add_report_line "- 100k_cross_layer_edges: $(metric_value "$latest_enterprise_100k_output" cross_layer_edges)"
fi
if [[ -f "$latest_enterprise_1m_output" ]]; then
  add_report_line "- 1m_dim: $enterprise_1m_dim"
  add_report_line "- 1m_vector_index_requested: $(metric_value "$latest_enterprise_1m_output" vector_index_requested)"
  add_report_line "- 1m_vector_index: $(metric_value "$latest_enterprise_1m_output" vector_index)"
  add_report_line "- 1m_causal_root_hit_rate: $(metric_value "$latest_enterprise_1m_output" causal_root_hit_rate)"
  add_report_line "- 1m_causal_lattice_p95_ms: $(metric_value "$latest_enterprise_1m_output" causal_lattice_p95_ms)"
  add_report_line "- 1m_metadata_service_p95_ms: $(metric_value "$latest_enterprise_1m_output" metadata_service_p95_ms)"
  add_report_line "- 1m_cross_layer_edges: $(metric_value "$latest_enterprise_1m_output" cross_layer_edges)"
fi
if [[ -f "$latest_enterprise_summary" ]]; then
  add_report_line "- full_ctest_completed: $(markdown_field_value "$latest_enterprise_summary" "full_ctest_completed")"
  add_report_line "- ga_readiness_completed: $(markdown_field_value "$latest_enterprise_summary" "ga_readiness_completed")"
  add_report_line "- fuzz_completed: $(markdown_field_value "$latest_enterprise_summary" "fuzz_completed")"
  add_report_line "- soak_completed: $(markdown_field_value "$latest_enterprise_summary" "soak_completed")"
  add_report_line "- filesystem_gate_passed: $(markdown_field_value "$latest_enterprise_summary" "filesystem_gate_passed")"
  add_report_line "- disk_pressure_gate_passed: $(markdown_field_value "$latest_enterprise_summary" "disk_pressure_gate_passed")"
  add_report_line "- full_day_soak_profile_ready: $(markdown_field_value "$latest_enterprise_summary" "full_day_soak_profile_ready")"
  add_report_line "- release_like_profile_ready: $(markdown_field_value "$latest_enterprise_summary" "release_like_profile_ready")"
  add_report_line "- graphenedb_use_faiss: $(markdown_field_value "$latest_enterprise_summary" "graphenedb_use_faiss")"
fi
add_report_line ""
add_report_line "## GA Host Attestation"
ga_host_status="MISSING"
[[ -f "$latest_ga_host_profile" ]] && ga_host_status="PARTIAL"
[[ "$approved_host" == "true" ]] && ga_host_status="PASS"
add_report_line "- [$ga_host_status] Host profile: \`$latest_ga_host_profile\`"
if [[ -f "$latest_ga_host_profile" ]]; then
  add_report_line "- approved_host: $approved_host"
  add_report_line "- profile_label: $ga_profile_label"
fi
add_report_line ""
add_report_line "## Latest Passing GA Benchmarks"
if [[ -f "$latest_ga_summary" ]]; then
  add_report_line "- graphenedb_use_faiss: $(markdown_field_value "$latest_ga_summary" "graphenedb_use_faiss")"
  add_report_line "- vector_index_recall_requested: $(markdown_field_value "$latest_ga_summary" "vector_index_recall_requested")"
  add_report_line "- vector_index_recall: $(markdown_field_value "$latest_ga_summary" "vector_index_recall")"
  add_report_line "- vector_index_recall_mean_recall_at_k: $(markdown_field_value "$latest_ga_summary" "vector_index_recall_mean_recall_at_k")"
  add_report_line "- extraction_vector_index_requested: $(markdown_field_value "$latest_ga_summary" "extraction_vector_index_requested")"
  add_report_line "- extraction_vector_index: $(markdown_field_value "$latest_ga_summary" "extraction_vector_index")"
  add_report_line "- storage_vector_index_requested: $(markdown_field_value "$latest_ga_summary" "storage_vector_index_requested")"
  add_report_line "- storage_vector_index: $(markdown_field_value "$latest_ga_summary" "storage_vector_index")"
else
  add_report_line "- [MISSING] Latest passing GA summary: \`$latest_ga_summary\`"
fi
add_report_line ""
add_report_line "## Latest GA Attempt"
ga_attempt_status="MISSING"
if [[ -f "$latest_ga_attempt_summary" ]]; then
  ga_attempt_status="PARTIAL"
  if [[ "$(grep -v '^$' "$latest_ga_attempt_summary" | tail -n 1)" == "PASS" ]]; then
    ga_attempt_status="PASS"
  fi
fi
add_report_line "- [$ga_attempt_status] Latest attempted GA summary: \`$latest_ga_attempt_summary\`"
ga_attempt_host_status="MISSING"
[[ -f "$latest_ga_attempt_host_profile" ]] && ga_attempt_host_status="PARTIAL"
add_report_line "- [$ga_attempt_host_status] Latest attempted GA host profile: \`$latest_ga_attempt_host_profile\`"
if [[ -f "$latest_ga_attempt_host_profile" ]]; then
  add_report_line "- profile_label: $ga_attempt_profile_label"
  add_report_line "- approved_host: $(json_field "$latest_ga_attempt_host_profile" approved_host)"
fi
add_report_line ""
add_report_line "## Preview Hardware Profile"
preview_pass="false"
if [[ -f "$latest_preview_profile_summary" ]] && [[ "$(grep -v '^$' "$latest_preview_profile_summary" | tail -n 1)" == "PASS" ]]; then
  preview_pass="true"
fi
status="MISSING"
[[ -f "$latest_preview_profile_summary" ]] && status="PARTIAL"
[[ "$preview_pass" == "true" ]] && status="PASS"
add_report_line "- [$status] Latest preview hardware summary: \`$latest_preview_profile_summary\`"
preview_host_status="MISSING"
[[ -f "$latest_preview_host_profile" ]] && preview_host_status="PARTIAL"
[[ "$intended_preview_hardware" == "true" ]] && preview_host_status="PASS"
add_report_line "- [$preview_host_status] Host profile: \`$latest_preview_host_profile\`"
if [[ -f "$latest_preview_host_profile" ]]; then
  add_report_line "- intended_hardware: $intended_preview_hardware"
  add_report_line "- profile_label: $preview_profile_label"
fi
if [[ -f "$latest_preview_profile_summary" ]]; then
  add_report_line "- graphenedb_use_faiss: $(markdown_field_value "$latest_preview_profile_summary" "graphenedb_use_faiss")"
fi
if [[ -f "$latest_preview_vector_recall_output" ]]; then
  add_report_line "- preview_vector_index_requested: $(metric_value "$latest_preview_vector_recall_output" vector_index_requested)"
  add_report_line "- preview_vector_index: $(metric_value "$latest_preview_vector_recall_output" vector_index)"
  add_report_line "- preview_mean_recall_at_k: $(metric_value "$latest_preview_vector_recall_output" mean_recall_at_k)"
fi
if [[ -f "$latest_preview_extraction_output" ]]; then
  add_report_line "- extraction_vector_index_requested: $(metric_value "$latest_preview_extraction_output" vector_index_requested)"
  add_report_line "- extraction_vector_index: $(metric_value "$latest_preview_extraction_output" vector_index)"
fi
if [[ -f "$latest_preview_storage_output" ]]; then
  add_report_line "- storage_vector_index_requested: $(metric_value "$latest_preview_storage_output" vector_index_requested)"
  add_report_line "- storage_vector_index: $(metric_value "$latest_preview_storage_output" vector_index)"
fi
add_report_line ""
add_report_line "## Pending For Public Developer Preview"
for item in "${should_fix[@]}"; do
  [[ -n "$item" ]] && add_report_line "- $item"
done
add_report_line ""
add_report_line "## Pending For Enterprise GA"
for item in "${must_fix[@]}"; do
  [[ -n "$item" ]] && add_report_line "- $item"
done
add_report_line ""
add_report_line "## Summary"
add_report_line "GrapheneDB has strong local evidence for controlled-pilot and release-candidate readiness, but enterprise GA is still blocked on long-running soak/fuzz, target-host/full-filesystem campaigns, target-scale performance, live integration decisions, and release governance."

echo "ga_status_report=$OUT"
