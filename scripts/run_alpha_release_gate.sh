#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${GRAPHENEDB_ALPHA_BUILD_DIR:-${ROOT_DIR}/build/alpha-release-gate}"
REPORT_DIR="${GRAPHENEDB_ALPHA_REPORT_DIR:-${ROOT_DIR}/reports/alpha-release-gate}"
JOBS="${GRAPHENEDB_JOBS:-2}"
CROSS_MODE="${GRAPHENEDB_CROSS_DATASET_MODE:-offline}"

case "${CROSS_MODE}" in
  offline|public|all) ;;
  *) echo "error: GRAPHENEDB_CROSS_DATASET_MODE must be offline, public or all" >&2; exit 2 ;;
esac

for command_name in cmake c++ python3 git; do
  command -v "${command_name}" >/dev/null 2>&1 || {
    echo "error: '${command_name}' is required but was not found in PATH" >&2
    exit 3
  }
done

rm -rf "${BUILD_DIR}" "${REPORT_DIR}"
mkdir -p "${BUILD_DIR}" "${REPORT_DIR}"

SOURCE_COMMIT="${GRAPHENEDB_SOURCE_COMMIT_OVERRIDE:-}"
if [[ -z "${SOURCE_COMMIT}" ]]; then
  SOURCE_COMMIT="$(git -C "${ROOT_DIR}" rev-parse HEAD)"
fi
if [[ ! "${SOURCE_COMMIT}" =~ ^[0-9a-fA-F]{40}$ && "${SOURCE_COMMIT}" != "unknown" ]]; then
  echo "error: source commit override must be a 40-character Git SHA or 'unknown'" >&2
  exit 4
fi
printf '%s\n' "${SOURCE_COMMIT}" > "${REPORT_DIR}/source_commit.txt"
{
  echo "cmake=$(cmake --version | head -n 1)"
  echo "compiler=$(c++ --version | head -n 1)"
  echo "python=$(python3 --version 2>&1)"
  echo "platform=$(uname -a 2>/dev/null || echo unknown)"
  echo "source_commit_override=${GRAPHENEDB_SOURCE_COMMIT_OVERRIDE:-}"
} > "${REPORT_DIR}/environment.txt"

printf '\n[1/7] Configure exact-head release build\n'
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON \
  2>&1 | tee "${REPORT_DIR}/01-configure.log"

printf '\n[2/7] Build all configured targets\n'
cmake --build "${BUILD_DIR}" --parallel "${JOBS}" \
  2>&1 | tee "${REPORT_DIR}/02-build.log"

printf '\n[3/7] Run complete CTest suite\n'
ctest --test-dir "${BUILD_DIR}" --output-on-failure \
  2>&1 | tee "${REPORT_DIR}/03-ctest.log"

printf '\n[4/7] Verify installed CMake package and external consumer\n'
GRAPHENEDB_VERIFY_DIR="${BUILD_DIR}/package-verify" \
GRAPHENEDB_KEEP_VERIFY_DIR=1 \
  bash "${ROOT_DIR}/scripts/verify_developer_install.sh" \
  2>&1 | tee "${REPORT_DIR}/04-package-consumer.log"

printf '\n[5/7] Run controlled dialectic intervention benchmark\n'
DIALECTIC_INTERVENTION_BUILD_DIR="${BUILD_DIR}/dialectic-intervention" \
DIALECTIC_INTERVENTION_REPORT_DIR="${REPORT_DIR}/dialectic-intervention" \
  bash "${ROOT_DIR}/scripts/run_dialectic_intervention.sh" \
  2>&1 | tee "${REPORT_DIR}/05-dialectic-intervention.log"

printf '\n[6/7] Run cross-dataset structural gate (%s)\n' "${CROSS_MODE}"
CROSS_DATASET_BUILD_DIR="${BUILD_DIR}/cross-dataset" \
CROSS_DATASET_REPORT_DIR="${REPORT_DIR}/cross-dataset" \
GRAPHENEDB_SOURCE_COMMIT_OVERRIDE="${SOURCE_COMMIT}" \
  bash "${ROOT_DIR}/scripts/run_cross_dataset_local.sh" "${CROSS_MODE}" \
  2>&1 | tee "${REPORT_DIR}/06-cross-dataset.log"

printf '\n[7/7] Write immutable manifest and checksums\n'
python3 - "${REPORT_DIR}" "${SOURCE_COMMIT}" "${CROSS_MODE}" <<'PY'
from __future__ import annotations
import hashlib
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
source_commit = sys.argv[2]
cross_mode = sys.argv[3]
files = []
for path in sorted(root.rglob('*')):
    if not path.is_file() or path.name in {'manifest.json', 'checksums.sha256'}:
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    files.append({'path': path.relative_to(root).as_posix(), 'sha256': digest, 'bytes': path.stat().st_size})
manifest = {
    'schema_version': 1,
    'source_commit': source_commit,
    'cross_dataset_mode': cross_mode,
    'status': 'PASS',
    'files': files,
}
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
(root / 'checksums.sha256').write_text(
    ''.join(f"{item['sha256']}  {item['path']}\n" for item in files),
    encoding='utf-8',
)
PY

printf '\nalpha_release_gate=PASS\nsource_commit=%s\nreport_dir=%s\n' \
  "${SOURCE_COMMIT}" "${REPORT_DIR}"
