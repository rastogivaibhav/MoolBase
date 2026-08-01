#!/usr/bin/env bash
set -euo pipefail

fail() {
  printf 'EVIDENCE_LAB_HOST_PREFLIGHT_FAILED: %s\n' "$1" >&2
  exit 1
}

command_required() {
  command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

case "$(uname -s)" in
  Linux) ;;
  *) fail "an x86-64 Linux host is required" ;;
esac

case "$(uname -m)" in
  x86_64|amd64) ;;
  *) fail "x86-64 architecture is required; found $(uname -m)" ;;
esac

for cmd in bash git cmake ninja c++ python3 curl node docker sha256sum; do
  command_required "$cmd"
done

python3 - <<'PY'
import sys
if sys.version_info < (3, 11):
    raise SystemExit("Python 3.11 or newer is required")
print("python=" + sys.version.split()[0])
PY

cmake_version="$(cmake --version | head -n1)"
compiler_version="$(c++ --version | head -n1)"
docker_version="$(docker version --format '{{.Server.Version}}' 2>/dev/null || true)"

[[ -n "$docker_version" ]] || fail "Docker daemon is unavailable to the runner user"
docker info >/dev/null 2>&1 || fail "Docker daemon cannot be queried by the runner user"

available_kb="$(df -Pk . | awk 'NR==2 {print $4}')"
[[ "$available_kb" =~ ^[0-9]+$ ]] || fail "could not determine available disk space"
(( available_kb >= 10485760 )) || fail "at least 10 GiB of free disk is required"

memory_kb="$(awk '/MemTotal:/ {print $2}' /proc/meminfo)"
[[ "$memory_kb" =~ ^[0-9]+$ ]] || fail "could not determine total memory"
(( memory_kb >= 4194304 )) || fail "at least 4 GiB of RAM is required"

ulimit -n 4096 2>/dev/null || true

cat <<EOF
EVIDENCE_LAB_HOST_PREFLIGHT=PASS
os=$(uname -sr)
architecture=$(uname -m)
${cmake_version}
compiler=${compiler_version}
docker=${docker_version}
free_disk_kib=${available_kb}
memory_kib=${memory_kb}
EOF
