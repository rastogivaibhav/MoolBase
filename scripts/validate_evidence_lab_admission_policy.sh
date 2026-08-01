#!/usr/bin/env bash
set -euo pipefail

WORKER_IMAGE="${1:-}"
NAMESPACE="${2:-graphenedb-evidence-lab}"
POLICY_NAME="graphenedb-evidence-lab-worker-jobs"
GATEWAY_IDENTITY="system:serviceaccount:${NAMESPACE}:evidence-lab-gateway"
TEST_JOB_NAME="evidence-lab-11111111111111111111"
TEST_INPUT="/var/lib/evidence-lab/jobs/${TEST_JOB_NAME}/input.json"
TEST_OUTPUT="/var/lib/evidence-lab/jobs/${TEST_JOB_NAME}/output.json"
TEST_COMMIT="1111111111111111111111111111111111111111"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

fail() {
  printf 'EVIDENCE_LAB_ADMISSION_POLICY_FAILED: %s\n' "$1" >&2
  exit 1
}

[[ "$WORKER_IMAGE" =~ ^ghcr\.io/rastogivaibhav/graphenedb-evidence-lab-worker@sha256:[0-9a-f]{64}$ ]] || \
  fail "worker image must be an immutable GrapheneDB worker digest"

TYPECHECK_COMPLETE=false
for attempt in $(seq 1 100); do
  set +e
  POLICY_JSON="$(kubectl get validatingadmissionpolicy "$POLICY_NAME" -o json 2>/dev/null)"
  FETCH_STATUS=$?
  set -e
  if [[ "$FETCH_STATUS" -ne 0 ]]; then
    sleep 0.2
    continue
  fi

  set +e
  TYPECHECK_OUTPUT="$(printf '%s' "$POLICY_JSON" | python3 -c '
import json
import sys

policy = json.load(sys.stdin)
type_checking = (policy.get("status") or {}).get("typeChecking")
if type_checking is None:
    raise SystemExit(2)
warnings = type_checking.get("expressionWarnings") or []
if warnings:
    print(json.dumps(warnings, indent=2, sort_keys=True))
    raise SystemExit(1)
print("EVIDENCE_LAB_ADMISSION_POLICY_TYPECHECK=PASS")
')"
  TYPECHECK_STATUS=$?
  set -e

  if [[ "$TYPECHECK_STATUS" -eq 0 ]]; then
    printf '%s\n' "$TYPECHECK_OUTPUT"
    TYPECHECK_COMPLETE=true
    break
  fi
  if [[ "$TYPECHECK_STATUS" -eq 1 ]]; then
    printf '%s\n' "$TYPECHECK_OUTPUT" >&2
    fail "Kubernetes reported CEL expression type-check warnings"
  fi
  sleep 0.2
done

[[ "$TYPECHECK_COMPLETE" == true ]] || \
  fail "Kubernetes did not complete admission-policy type checking"

kubectl auth can-i create jobs.batch \
  --namespace "$NAMESPACE" \
  --as "$GATEWAY_IDENTITY" | grep -Fx yes >/dev/null || \
  fail "gateway service account cannot create worker jobs"

cat > "$WORK_DIR/valid-worker.yaml" <<YAML
apiVersion: batch/v1
kind: Job
metadata:
  name: ${TEST_JOB_NAME}
  namespace: ${NAMESPACE}
  labels:
    app.kubernetes.io/name: graphenedb-evidence-lab-worker
    app.kubernetes.io/component: worker
spec:
  backoffLimit: 0
  activeDeadlineSeconds: 60
  ttlSecondsAfterFinished: 60
  template:
    metadata:
      labels:
        app.kubernetes.io/name: graphenedb-evidence-lab-worker
        app.kubernetes.io/component: worker
    spec:
      serviceAccountName: evidence-lab-worker
      automountServiceAccountToken: false
      enableServiceLinks: false
      restartPolicy: Never
      securityContext:
        runAsNonRoot: true
        runAsUser: 10001
        runAsGroup: 10001
        fsGroup: 10001
        seccompProfile:
          type: RuntimeDefault
      imagePullSecrets:
        - name: ghcr-pull
      containers:
        - name: worker
          image: ${WORKER_IMAGE}
          command: ["python", "-m", "evidence_lab.worker_entrypoint"]
          env:
            - name: EVIDENCE_LAB_JOB_INPUT
              value: ${TEST_INPUT}
            - name: EVIDENCE_LAB_JOB_OUTPUT
              value: ${TEST_OUTPUT}
            - name: EVIDENCE_LAB_BACKEND
              value: live
            - name: GRAPHENEDB_SERVER_BINARY
              value: /opt/graphenedb/bin/graphenedb_server
            - name: GRAPHENEDB_SOURCE_COMMIT
              value: ${TEST_COMMIT}
            - name: GRAPHENEDB_PUBLIC_VERSION
              value: v0.6.0-alpha.1-admission
            - name: EVIDENCE_LAB_RUN_TIMEOUT_SECONDS
              value: "60"
            - name: EVIDENCE_LAB_PUBLIC_MODE
              value: "true"
            - name: TMPDIR
              value: /tmp
          resources:
            requests:
              cpu: 250m
              memory: 256Mi
            limits:
              cpu: "1"
              memory: 1Gi
              ephemeral-storage: 512Mi
          securityContext:
            allowPrivilegeEscalation: false
            privileged: false
            readOnlyRootFilesystem: true
            runAsNonRoot: true
            runAsUser: 10001
            runAsGroup: 10001
            capabilities:
              drop: ["ALL"]
            seccompProfile:
              type: RuntimeDefault
          volumeMounts:
            - name: evidence-lab-data
              mountPath: /var/lib/evidence-lab
            - name: tmp
              mountPath: /tmp
      volumes:
        - name: evidence-lab-data
          persistentVolumeClaim:
            claimName: evidence-lab-data
        - name: tmp
          emptyDir:
            medium: Memory
            sizeLimit: 512Mi
YAML

kubectl create --dry-run=server \
  --as "$GATEWAY_IDENTITY" \
  -f "$WORK_DIR/valid-worker.yaml" >/dev/null

sed \
  -e 's/name: evidence-lab-11111111111111111111/name: evidence-lab-22222222222222222222/' \
  -e 's/serviceAccountName: evidence-lab-worker/serviceAccountName: evidence-lab-gateway/' \
  -e 's/automountServiceAccountToken: false/automountServiceAccountToken: true/' \
  "$WORK_DIR/valid-worker.yaml" > "$WORK_DIR/invalid-worker.yaml"

set +e
DENIAL_OUTPUT="$(kubectl create --dry-run=server \
  --as "$GATEWAY_IDENTITY" \
  -f "$WORK_DIR/invalid-worker.yaml" 2>&1)"
DENIAL_STATUS=$?
set -e

if [[ "$DENIAL_STATUS" -eq 0 ]]; then
  fail "admission policy accepted a worker using the gateway service account"
fi
printf '%s\n' "$DENIAL_OUTPUT" | grep -E \
  'graphenedb-evidence-lab-worker-jobs|tokenless worker service account|tokenless evidence-lab-worker' >/dev/null || \
  fail "invalid Job was denied, but not demonstrably by the Evidence Lab admission policy"

printf 'EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS\n'
