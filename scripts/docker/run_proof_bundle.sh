#!/usr/bin/env bash
set -Eeuo pipefail

MODE="${1:-smoke}"
case "${MODE}" in
  smoke|full|security|public|api-smoke) ;;
  *) echo "usage: $0 [smoke|full|security|public|api-smoke]" >&2; exit 2 ;;
esac

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EVIDENCE_ROOT="${GRAPHENEDB_EVIDENCE_DIR:-/evidence}"
SOURCE_COMMIT="${GRAPHENEDB_SOURCE_COMMIT:-unknown}"
SOURCE_REF="${GRAPHENEDB_SOURCE_REF:-master}"
STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
RUN_ID="${SOURCE_COMMIT:0:12}-${MODE}"
OUT_DIR="${EVIDENCE_ROOT}/${RUN_ID}"
WORK_DIR="/tmp/graphenedb-proof-${RUN_ID}"
JOBS="${GRAPHENEDB_JOBS:-2}"
FINAL_STATUS="FAIL"

rm -rf "${OUT_DIR}" "${WORK_DIR}"
mkdir -p "${OUT_DIR}" "${WORK_DIR}"
exec > >(tee -a "${OUT_DIR}/proof.log") 2>&1

export GRAPHENEDB_SOURCE_COMMIT_OVERRIDE="${SOURCE_COMMIT}"

write_environment() {
  {
    echo "source_commit=${SOURCE_COMMIT}"
    echo "source_ref=${SOURCE_REF}"
    echo "mode=${MODE}"
    echo "started_at=${STARTED_AT}"
    echo "platform=$(uname -a 2>/dev/null || true)"
    echo "cmake=$(cmake --version | head -n 1)"
    echo "compiler=$(c++ --version | head -n 1)"
    echo "clang=$(clang++ --version | head -n 1)"
    echo "python=$(python3 --version 2>&1)"
  } > "${OUT_DIR}/environment.txt"
}

finalise() {
  local rc=$?
  trap - EXIT
  local finished_at
  finished_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  python3 "${ROOT_DIR}/scripts/docker/create_proof_manifest.py" \
    --output-dir "${OUT_DIR}" \
    --status "${FINAL_STATUS}" \
    --mode "${MODE}" \
    --source-commit "${SOURCE_COMMIT}" \
    --source-ref "${SOURCE_REF}" \
    --started-at "${STARTED_AT}" \
    --finished-at "${finished_at}" || rc=$?
  (
    cd "${OUT_DIR}"
    find . -type f ! -name checksums.sha256 -print0 \
      | sort -z | xargs -0 sha256sum > checksums.sha256
  )
  tar -C "${OUT_DIR}" -czf "${EVIDENCE_ROOT}/graphenedb-proof-${RUN_ID}.tar.gz" .
  echo "proof_status=${FINAL_STATUS}"
  echo "proof_mode=${MODE}"
  echo "proof_evidence=${OUT_DIR}"
  echo "proof_archive=${EVIDENCE_ROOT}/graphenedb-proof-${RUN_ID}.tar.gz"
  exit "${rc}"
}
trap finalise EXIT

step() {
  local name="$1"
  shift
  echo
  echo "==> ${name}"
  printf '%q ' "$@" >> "${OUT_DIR}/commands.log"
  printf '\n' >> "${OUT_DIR}/commands.log"
  "$@" 2>&1 | tee "${OUT_DIR}/${name}.log"
}

run_smoke() {
  local build="${WORK_DIR}/smoke-build"
  step 01-configure cmake -S "${ROOT_DIR}" -B "${build}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGRAPHENEDB_BUILD_TESTS=ON \
    -DGRAPHENEDB_BUILD_SERVER=ON \
    -DGRAPHENEDB_BUILD_BENCH=ON \
    -DGRAPHENEDB_BUILD_EXAMPLES=ON
  step 02-build cmake --build "${build}" --parallel "${JOBS}" --target \
    graphenedb_hypokosh_runtime_demo \
    graphenedb_server graphenedb_healthcheck \
    graphenedb_hypokosh_runtime_tests \
    graphenedb_fiber_bundle_v2_tests \
    graphenedb_epistemic_lineage_tests \
    graphenedb_epistemic_control_tests \
    graphenedb_epistemic_receipt_tests \
    graphenedb_lyapunov_critic_tests \
    graphenedb_stability_critic_adversarial_tests
  step 03-critical-ctest ctest --test-dir "${build}" --output-on-failure \
    --output-junit "${OUT_DIR}/critical-ctest.xml" \
    -R 'graphenedb_(hypokosh_runtime|fiber_bundle_v2|epistemic_lineage|epistemic_control|epistemic_receipt|lyapunov_critic|stability_critic_adversarial)_tests'
  step 04-demo "${build}/graphenedb_hypokosh_runtime_demo"
  step 05-live-server-contract python3 \
    "${ROOT_DIR}/scripts/server_hypokosh_runtime_contract_test.py" \
    "${build}/graphenedb_server"
  step 06-intervention env \
    DIALECTIC_INTERVENTION_BUILD_DIR="${WORK_DIR}/dialectic-build" \
    DIALECTIC_INTERVENTION_REPORT_DIR="${OUT_DIR}/dialectic-intervention" \
    DIALECTIC_INTERVENTION_REPETITIONS="${DIALECTIC_INTERVENTION_REPETITIONS:-20}" \
    GRAPHENEDB_JOBS="${JOBS}" \
    bash "${ROOT_DIR}/scripts/run_dialectic_intervention.sh"
  step 07-cross-dataset env \
    CROSS_DATASET_BUILD_DIR="${WORK_DIR}/cross-dataset-build" \
    CROSS_DATASET_REPORT_DIR="${OUT_DIR}/cross-dataset" \
    GRAPHENEDB_SOURCE_COMMIT_OVERRIDE="${SOURCE_COMMIT}" \
    bash "${ROOT_DIR}/scripts/run_cross_dataset_local.sh" offline
}

run_full() {
  step 01-alpha-release-gate env \
    GRAPHENEDB_ALPHA_BUILD_DIR="${WORK_DIR}/alpha-build" \
    GRAPHENEDB_ALPHA_REPORT_DIR="${OUT_DIR}/alpha-release-gate" \
    GRAPHENEDB_CROSS_DATASET_MODE=offline \
    GRAPHENEDB_SOURCE_COMMIT_OVERRIDE="${SOURCE_COMMIT}" \
    GRAPHENEDB_JOBS="${JOBS}" \
    bash "${ROOT_DIR}/scripts/run_alpha_release_gate.sh"

  local server_build="${WORK_DIR}/server-build"
  step 02-server-configure cmake -S "${ROOT_DIR}" -B "${server_build}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGRAPHENEDB_BUILD_TESTS=ON \
    -DGRAPHENEDB_BUILD_SERVER=ON \
    -DGRAPHENEDB_BUILD_BENCH=OFF \
    -DGRAPHENEDB_BUILD_EXAMPLES=OFF
  step 03-server-build cmake --build "${server_build}" --parallel "${JOBS}" \
    --target graphenedb_server graphenedb_healthcheck
  step 04-server-contract python3 \
    "${ROOT_DIR}/scripts/server_hypokosh_runtime_contract_test.py" \
    "${server_build}/graphenedb_server"
}

run_security() {
  step 01-sanitizers env \
    JOBS="${JOBS}" \
    ASAN_BUILD_DIR="${WORK_DIR}/asan-ubsan" \
    TSAN_BUILD_DIR="${WORK_DIR}/tsan" \
    bash "${ROOT_DIR}/scripts/run_sanitizers.sh"
  step 02-fuzz env \
    RUNS="${GRAPHENEDB_FUZZ_RUNS:-10000}" \
    FUZZ_BUILD_DIR="${WORK_DIR}/fuzz" \
    bash "${ROOT_DIR}/scripts/run_fuzz_smoke.sh"
}

run_public() {
  step 01-public-cross-dataset env \
    CROSS_DATASET_BUILD_DIR="${WORK_DIR}/cross-dataset-build" \
    CROSS_DATASET_REPORT_DIR="${OUT_DIR}/cross-dataset" \
    GRAPHENEDB_SOURCE_COMMIT_OVERRIDE="${SOURCE_COMMIT}" \
    BABI_PER_TASK="${BABI_PER_TASK:-200}" \
    HOTPOT="${HOTPOT:-500}" \
    FEVER_PER_LABEL="${FEVER_PER_LABEL:-200}" \
    CROSS_DATASET_SEED="${CROSS_DATASET_SEED:-20260729}" \
    bash "${ROOT_DIR}/scripts/run_cross_dataset_local.sh" public
}

write_environment
case "${MODE}" in
  smoke) run_smoke ;;
  full) run_full ;;
  security) run_security ;;
  public) run_public ;;
  api-smoke)
    step 01-api-smoke python3 "${ROOT_DIR}/scripts/docker/remote_runtime_smoke.py" \
      --base-url "${GRAPHENEDB_SERVER_URL:-http://graphenedb:8080}" \
      --api-key "${GRAPHENEDB_API_KEY:-local-development-key}" \
      --output "${OUT_DIR}/api-smoke.json"
    ;;
esac
FINAL_STATUS="PASS"
