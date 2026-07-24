#!/usr/bin/env bash
set -uo pipefail

# This runner intentionally requires prepared, pinned inputs. The first
# evaluation command creates the immutable manifest; no gate command runs
# before it.

ROOT="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd -P)"
: "${RUN_DIR:?RUN_DIR must be an absolute directory outside the source checkout}"
: "${RUN_ID:?RUN_ID must be a preregistered immutable run identifier}"
: "${D0_EVIDENCE_DIR:?D0_EVIDENCE_DIR must contain prepared dataset.jsonl and run_config.txt}"
: "${D1_CORPUS:?D1_CORPUS must be a clean checkout of the pinned D1 corpus}"
: "${D1_COMMIT:?D1_COMMIT must be the full pinned D1 commit}"
: "${IMAGE_REF:?IMAGE_REF must name the already-built pinned production image}"
: "${IMAGE_DIGEST:?IMAGE_DIGEST must be the image ID sha256 digest}"
: "${HARDWARE_PROFILE:?HARDWARE_PROFILE must be a prepared immutable file}"
: "${EXTERNAL_VERSIONS:?EXTERNAL_VERSIONS must be a prepared immutable file}"

case "$RUN_DIR" in
  /*) ;;
  *) echo "RUN_DIR must be absolute" >&2; exit 64 ;;
esac
case "$RUN_DIR/" in
  "$ROOT/"*) echo "RUN_DIR must be outside the source checkout" >&2; exit 64 ;;
esac

[[ -f "$D0_EVIDENCE_DIR/dataset.jsonl" ]] || {
  echo "prepared D0 dataset is missing" >&2
  exit 64
}
[[ -f "$D0_EVIDENCE_DIR/run_config.txt" ]] || {
  echo "prepared D0 run configuration is missing" >&2
  exit 64
}

PYTHON="${PYTHON:-python3}"
JOBS="${JOBS:-2}"
SEED="${SEED:-20260724}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-deepmind-foundation}"
MANIFEST="$RUN_DIR/run_manifest.json"
MANIFEST_TOOL="$ROOT/scripts/deepmind_foundation_manifest.py"
RETRIEVAL_CONFIG="$ROOT/bench/deepmind/retrieval_config.json"
TOOL_PERMISSIONS="$ROOT/bench/deepmind/tool_permissions.json"

# First evaluation command: it both records the start time and locks all
# identities. It fails closed on a dirty or untracked source checkout.
"$PYTHON" "$MANIFEST_TOOL" create \
  --repo "$ROOT" \
  --output "$MANIFEST" \
  --run-id "$RUN_ID" \
  --d0-dataset "$D0_EVIDENCE_DIR/dataset.jsonl" \
  --d1-corpus "$D1_CORPUS" \
  --d1-commit "$D1_COMMIT" \
  --image-ref "$IMAGE_REF" \
  --image-digest "$IMAGE_DIGEST" \
  --hardware-profile "$HARDWARE_PROFILE" \
  --tool-permissions "$TOOL_PERMISSIONS" \
  --external-versions "$EXTERNAL_VERSIONS" \
  --retrieval-config "$RETRIEVAL_CONFIG" \
  --policy-version "dialectic-empirical-d0-v1" \
  --seed "$SEED" || exit $?

mkdir -p "$RUN_DIR/logs"

declare -A RESULT=(
  [ctest]=false
  [openapi_routes]=false
  [package_consumer]=false
  [docker_non_root]=false
  [clean_checkout_rerun]=false
  [d0_g2]=false
  [git_diff_check]=false
)

run_logged() {
  local name="$1"
  shift
  local log="$RUN_DIR/logs/$name.log"
  if "$@" >"$log" 2>&1; then
    cat "$log"
    return 0
  fi
  local code=$?
  cat "$log" >&2
  echo "foundation_step_failed=$name exit_code=$code" >&2
  return "$code"
}

verify_docker_non_root() {
  local actual
  actual="$(docker image inspect "$IMAGE_REF" --format '{{.Id}}')" || return 1
  [[ "$actual" == "$IMAGE_DIGEST" ]] || {
    echo "image digest mismatch: actual=$actual expected=$IMAGE_DIGEST" >&2
    return 1
  }
  local container="graphenedb-g0-${RUN_ID//[^a-zA-Z0-9_.-]/-}"
  if docker container inspect "$container" >/dev/null 2>&1; then
    echo "refusing to reuse existing Docker container: $container" >&2
    return 1
  fi
  docker run -d \
    --name "$container" \
    --read-only \
    --tmpfs /tmp:rw,noexec,nosuid,size=16m,uid=10001,gid=10001 \
    --tmpfs /var/lib/graphenedb:rw,nosuid,size=128m,uid=10001,gid=10001,mode=0700 \
    -e GRAPHENEDB_API_KEY=g0-foundation-validation-only \
    "$IMAGE_REF" >/dev/null || return 1

  local healthy=false
  for _ in {1..30}; do
    if docker exec "$container" /usr/local/bin/graphenedb_healthcheck \
        127.0.0.1 8080 >/dev/null 2>&1; then
      healthy=true
      break
    fi
    if [[ "$(docker inspect "$container" --format '{{.State.Running}}')" != "true" ]]; then
      break
    fi
    sleep 1
  done

  local uid gid runtime
  uid="$(docker exec "$container" id -u 2>/dev/null || true)"
  gid="$(docker exec "$container" id -g 2>/dev/null || true)"
  runtime="$(docker inspect "$container" \
    --format '{{.HostConfig.ReadonlyRootfs}} {{.Config.User}}' 2>/dev/null || true)"
  if [[ "$healthy" != true || "$uid" != 10001 || "$gid" != 10001 ||
        "$runtime" != "true 10001:10001" ]]; then
    docker logs "$container" >&2 || true
    docker rm -f "$container" >/dev/null 2>&1 || true
    return 1
  fi
  docker logs "$container"
  docker rm -f "$container" >/dev/null
}

if run_logged manifest_running "$PYTHON" "$MANIFEST_TOOL" validate \
    --manifest "$MANIFEST" --check-live; then
  RESULT[clean_checkout_rerun]=true
fi

run_logged configure cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=OFF
configure_status=$?
if [[ "$configure_status" -eq 0 ]]; then
  run_logged build cmake --build "$BUILD_DIR" -j "$JOBS"
  build_status=$?
else
  build_status=1
fi

if [[ "$build_status" -eq 0 ]] &&
   run_logged ctest ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$JOBS"; then
  RESULT[ctest]=true
fi

if run_logged openapi_routes "$PYTHON" "$ROOT/tests/test_openapi_surface.py" \
    "$ROOT/tools/graphenedb_server.cpp" "$ROOT/docs/api/openapi-v1.yaml"; then
  RESULT[openapi_routes]=true
fi

if run_logged package_consumer env \
    BUILD_DIR=build-deepmind-package \
    INSTALL_DIR=build-deepmind-install \
    CONSUMER_BUILD_DIR=build-deepmind-consumer \
    JOBS="$JOBS" \
    bash "$ROOT/scripts/verify_package_install.sh"; then
  RESULT[package_consumer]=true
fi

if run_logged docker_non_root verify_docker_non_root; then
  RESULT[docker_non_root]=true
fi

if [[ "$build_status" -eq 0 ]] &&
   run_logged d0_g2 "$BUILD_DIR/graphenedb_deepmind_causal_suite" \
     --output-dir "$D0_EVIDENCE_DIR" \
     --seed "$SEED" \
     --graphs 5000 \
     --queries-per-graph 4 \
     --wall-clock-seconds "${D0_WALL_CLOCK_SECONDS:-900}" \
     --per-query-ms "${D0_PER_QUERY_MS:-250}" \
     --resume \
     --formal; then
  RESULT[d0_g2]=true
fi

if run_logged git_diff_check git -C "$ROOT" diff --check; then
  clean_status="$(git -C "$ROOT" status --porcelain=v1 --untracked-files=all)"
  if [[ -z "$clean_status" ]]; then
    RESULT[git_diff_check]=true
  else
    echo "$clean_status" | tee -a "$RUN_DIR/logs/git_diff_check.log" >&2
  fi
fi

artifact_args=()
for artifact in manifest_running configure build ctest openapi_routes package_consumer docker_non_root d0_g2 git_diff_check; do
  log="$RUN_DIR/logs/$artifact.log"
  [[ -f "$log" ]] && artifact_args+=(--artifact "$artifact=$log")
done
[[ -f "$D0_EVIDENCE_DIR/metrics.json" ]] &&
  artifact_args+=(--artifact "d0_metrics=$D0_EVIDENCE_DIR/metrics.json")
[[ -f "$D0_EVIDENCE_DIR/raw_predictions.jsonl" ]] &&
  artifact_args+=(--artifact "d0_raw_predictions=$D0_EVIDENCE_DIR/raw_predictions.jsonl")

"$PYTHON" "$MANIFEST_TOOL" finalize \
  --manifest "$MANIFEST" \
  --result "ctest=${RESULT[ctest]}" \
  --result "openapi_routes=${RESULT[openapi_routes]}" \
  --result "package_consumer=${RESULT[package_consumer]}" \
  --result "docker_non_root=${RESULT[docker_non_root]}" \
  --result "clean_checkout_rerun=${RESULT[clean_checkout_rerun]}" \
  --result "d0_g2=${RESULT[d0_g2]}" \
  --result "git_diff_check=${RESULT[git_diff_check]}" \
  "${artifact_args[@]}"
final_status=$?

if [[ "$final_status" -eq 0 ]]; then
  "$PYTHON" "$MANIFEST_TOOL" validate \
    --manifest "$MANIFEST" --check-live --require-complete
fi
exit "$final_status"
