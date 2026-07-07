#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${OUT:-reports/HOST_PROFILE.json}"
PROFILE_LABEL="${PROFILE_LABEL:-}"
INTENDED_HARDWARE="${INTENDED_HARDWARE:-0}"
APPROVED_HOST="${APPROVED_HOST:-0}"

case "$OUT" in
  /*) OUT_PATH="$OUT" ;;
  *) OUT_PATH="$ROOT/$OUT" ;;
esac

mkdir -p "$(dirname "$OUT_PATH")"

hostname_value="$(hostname 2>/dev/null || echo unknown)"
os_name="$(uname -s 2>/dev/null || echo unknown)"
os_release="$(uname -r 2>/dev/null || echo unknown)"
machine="$(uname -m 2>/dev/null || echo unknown)"
cpu_model="$( (lscpu 2>/dev/null | awk -F: '/Model name/ {gsub(/^[ \t]+/, "", $2); print $2; exit}') || true)"
logical_cores="$( (getconf _NPROCESSORS_ONLN 2>/dev/null) || echo 0)"
physical_cores="$( (lscpu 2>/dev/null | awk -F: '/Core\(s\) per socket/ {gsub(/^[ \t]+/, "", $2); c=$2} /Socket\(s\)/ {gsub(/^[ \t]+/, "", $2); s=$2} END {if (c && s) print c*s}') || true)"
mem_bytes="$( (awk '/MemTotal:/ {print $2 * 1024; exit}' /proc/meminfo 2>/dev/null) || true)"
cmake_ver="$(cmake --version 2>/dev/null | head -n 1 || true)"
clang_ver="$(clang --version 2>/dev/null | head -n 1 || true)"
clangxx_ver="$(clang++ --version 2>/dev/null | head -n 1 || true)"
gcc_ver="$(g++ --version 2>/dev/null | head -n 1 || true)"

python3 - "$OUT_PATH" "$PROFILE_LABEL" "$INTENDED_HARDWARE" "$APPROVED_HOST" "$hostname_value" "$os_name" "$os_release" "$machine" "$cpu_model" "${logical_cores:-0}" "${physical_cores:-0}" "${mem_bytes:-0}" "$cmake_ver" "$clang_ver" "$clangxx_ver" "$gcc_ver" <<'PY'
import datetime as dt
import json
import pathlib
import sys

(out_path, profile_label, intended_hardware, approved_host, hostname_value, os_name, os_release, machine,
 cpu_model, logical_cores, physical_cores, mem_bytes, cmake_ver, clang_ver, clangxx_ver, gcc_ver) = sys.argv[1:]

def maybe_int(value: str) -> int:
    try:
        return int(float(value))
    except Exception:
        return 0

payload = {
    "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
    "platform": "posix",
    "profile_label": profile_label,
    "intended_hardware": intended_hardware == "1",
    "approved_host": approved_host == "1",
    "hostname": hostname_value,
    "os": {
        "name": os_name,
        "release": os_release,
        "machine": machine,
    },
    "cpu": {
        "name": cpu_model,
        "logical_cores": maybe_int(logical_cores),
        "physical_cores": maybe_int(physical_cores),
    },
    "memory": {
        "total_physical_bytes": maybe_int(mem_bytes),
    },
    "toolchain": {
        "cmake": cmake_ver,
        "clang": clang_ver,
        "clangxx": clangxx_ver,
        "gcc": gcc_ver,
    },
}

pathlib.Path(out_path).write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

echo "host_profile=$OUT_PATH"
