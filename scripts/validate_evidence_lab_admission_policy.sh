#!/usr/bin/env bash
set -euo pipefail

WORKER_IMAGE="${1:-}"
NAMESPACE="${2:-graphenedb-evidence-lab}"
POLICY_NAME="graphenedb-evidence-lab-worker-jobs"
GATEWAY_IDENTITY="system:serviceaccount:${NAMESPACE}:evidence-lab-gateway"

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

cat <<YAML | kubectl apply --server-side --dry-run=server \
  --as "$GATEWAY_IDENTITY" -f - >/dev/null
apiVersion: batch/v1
kind: Job
metadata:
  name: evidence-lab-admission-valid
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
          resources:
            requests:
              cpu: 100m
              memory: 128Mi
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

set +e
DENIAL_OUTPUT="$(cat <<YAML | kubectl apply --server-side --dry-run=server \
  --as "$GATEWAY_IDENTITY" -f - 2>&1
apiVersion: batch/v1
kind: Job
metadata:
  name: evidence-lab-admission-invalid
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
      serviceAccountName: evidence-lab-gateway
      automountServiceAccountToken: true
      enableServiceLinks: false
      restartPolicy: Never
      securityContext:
        runAsNonRoot: true
        runAsUser: 10001
        runAsGroup: 10001
        seccompProfile:
          type: RuntimeDefault
      containers:
        - name: worker
          image: busybox:latest
          command: ["sh", "-c", "true"]
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
)"
DENIAL_STATUS=$?
set -e

if [[ "$DENIAL_STATUS" -eq 0 ]]; then
  fail "admission policy accepted an arbitrary gateway-created Job"
fi
printf '%s\n' "$DENIAL_OUTPUT" | grep -E \
  'graphenedb-evidence-lab-worker-jobs|immutable GrapheneDB worker|tokenless evidence-lab-worker' >/dev/null || \
  fail "invalid Job was denied, but not demonstrably by the Evidence Lab admission policy"

printf 'EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS\n'
