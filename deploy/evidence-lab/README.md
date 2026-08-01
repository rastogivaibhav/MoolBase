# GrapheneDB Evidence Lab deployment

This deployment keeps ChatGPT Sites as the public user interface and exposes only the Evidence Lab gateway. The GrapheneDB pilot server is never internet-facing.

## Security architecture

- uploads are extension, signature, archive, credential and ClamAV scanned before parsing;
- dataset archives reject traversal, symlinks, nested archives, executable-like extras, duplicate basenames and excessive expansion ratios;
- the gateway has no GrapheneDB binary in the production profile;
- every run creates one restricted Kubernetes Job;
- the gateway service account is constrained by a `ValidatingAdmissionPolicy`: it may create only the immutable, tokenless GrapheneDB worker shape;
- the worker has no service-account token;
- a deny-all NetworkPolicy blocks worker ingress and egress;
- the worker root filesystem is read-only and all Linux capabilities are dropped;
- CPU, memory, ephemeral-storage and wall-clock limits are applied;
- the job and run directory are deleted after result collection;
- anonymous sessions expire after one hour and support immediate deletion;
- private registry credentials are held in a namespace-scoped image-pull secret.

## Prerequisites

- Kubernetes 1.30 or newer with Pod Security Admission and `ValidatingAdmissionPolicy` enabled;
- deployment credentials allowed to create cluster-scoped admission policies and impersonate the gateway service account for the policy self-test;
- an ingress controller and TLS issuer;
- a `ReadWriteMany` storage class for the private gateway/worker exchange PVC;
- a container registry accessible by the cluster;
- DNS for the public gateway hostname;
- the final ChatGPT Sites origin URL;
- a GHCR credential with `read:packages` for cluster image pulls.

The committed manifest uses `evidence-lab.example.com`; the deployment workflow renders and rejects that placeholder before applying.

## Recommended deployment: guarded GitHub workflow

Configure a protected GitHub environment named:

```text
evidence-lab-production
```

Add these environment secrets:

```text
EVIDENCE_LAB_KUBE_CONFIG_B64
EVIDENCE_LAB_GHCR_USERNAME
EVIDENCE_LAB_GHCR_PAT
```

`EVIDENCE_LAB_KUBE_CONFIG_B64` is a base64-encoded kubeconfig scoped to the target cluster. The GHCR PAT requires package-read access and is used only to create the Kubernetes `ghcr-pull` secret. The workflow's short-lived `GITHUB_TOKEN` pushes the images.

Run **Deploy Evidence Lab** manually and provide:

- public gateway hostname;
- published ChatGPT Sites HTTPS origin;
- public release version;
- exact 40-character GrapheneDB source commit.

The workflow then:

1. validates all inputs, secrets and Kubernetes 1.30+;
2. builds and pushes gateway and exact-source worker images;
3. resolves gateway, worker and ClamAV image digests;
4. renders and preflights the Kubernetes resources;
5. installs the worker Job admission guard;
6. creates private-registry and application secrets;
7. proves that the admission guard accepts the exact worker and rejects an arbitrary Job;
8. deploys ClamAV and the gateway;
9. waits for readiness;
10. runs two samples and one uploaded dataset through live GrapheneDB;
11. verifies all reproduction-bundle checksums;
12. saves deployment and admission-policy evidence as a workflow artifact.

No deployment is represented as successful unless both markers are produced:

```text
EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS
EVIDENCE_LAB_LIVE_VALIDATION=PASS
```

## Manual image build

Run from the repository root:

```bash
docker build \
  -f apps/evidence_lab/gateway/Dockerfile.production \
  -t ghcr.io/rastogivaibhav/graphenedb-evidence-lab-gateway:0.2.0 .

docker build \
  -f apps/evidence_lab/worker/Dockerfile \
  -t ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker:0.2.0 .
```

Push the images, resolve their immutable digests and use only `@sha256:` references for public deployment.

## Manual deployment configuration

```bash
cp deploy/evidence-lab/kubernetes/evidence-lab-secrets.example.yaml \
   /tmp/evidence-lab-secrets.yaml
```

Edit the copy and replace:

- ChatGPT Sites origin;
- public gateway hostname;
- immutable worker image digest;
- exact verified GrapheneDB commit;
- public release version.

Create the namespace and private registry secret:

```bash
kubectl create namespace graphenedb-evidence-lab --dry-run=client -o yaml | kubectl apply -f -
kubectl -n graphenedb-evidence-lab create secret docker-registry ghcr-pull \
  --docker-server=ghcr.io \
  --docker-username="$GHCR_USERNAME" \
  --docker-password="$GHCR_PAT"
```

Do not commit populated secrets.

## Manual preflight and apply

Render the hostname before applying:

```bash
sed 's/evidence-lab.example.com/evidence.your-domain.com/g' \
  deploy/evidence-lab/kubernetes/evidence-lab.yaml \
  > /tmp/evidence-lab.yaml

python scripts/preflight_evidence_lab_deployment.py \
  /tmp/evidence-lab.yaml \
  --hostname evidence.your-domain.com \
  --site-origin https://YOUR-PUBLISHED-CHATGPT-SITE \
  --expected-commit FULL_40_CHARACTER_COMMIT

kubectl apply --dry-run=client \
  -f /tmp/evidence-lab.yaml \
  -f deploy/evidence-lab/kubernetes/cleanup-cronjob.yaml \
  -f deploy/evidence-lab/kubernetes/worker-admission-policy.yaml

kubectl apply -f deploy/evidence-lab/kubernetes/worker-admission-policy.yaml
kubectl apply \
  -f /tmp/evidence-lab.yaml \
  -f deploy/evidence-lab/kubernetes/cleanup-cronjob.yaml
kubectl apply -f /tmp/evidence-lab-secrets.yaml

bash scripts/validate_evidence_lab_admission_policy.sh \
  'ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker@sha256:FULL_DIGEST'

kubectl -n graphenedb-evidence-lab rollout status deployment/evidence-lab-clamav
kubectl -n graphenedb-evidence-lab rollout status deployment/evidence-lab-gateway
```

Check:

```bash
kubectl get validatingadmissionpolicy,validatingadmissionpolicybinding
kubectl -n graphenedb-evidence-lab get pods,job,svc,ingress,networkpolicy
curl -fsS https://evidence.your-domain.com/v1/public/health | python -m json.tool
```

## Run the public live gate

```bash
python -m pip install 'httpx>=0.27,<1'
python scripts/validate_evidence_lab_public.py \
  https://evidence.your-domain.com \
  --expected-commit FULL_40_CHARACTER_COMMIT
```

A release is not valid unless the command executes:

- two sample packs;
- one uploaded structured dataset;
- live GrapheneDB receipts;
- three downloadable reproduction bundles with valid checksums;
- immediate session deletion.

## Connect ChatGPT Sites

Set the Site's backend base URL to the gateway HTTPS origin. The browser must never receive a permanent gateway token. Add the published Site origin to `EVIDENCE_LAB_ALLOWED_ORIGINS` and the gateway hostname to `EVIDENCE_LAB_ALLOWED_HOSTS`.

The reference static frontend reads:

```javascript
window.GRAPHENEDB_EVIDENCE_LAB_API
```

Use the same public API contract from ChatGPT Sites. `apps/evidence_lab/site/config.example.js` is a deployment template.

Before publishing, replace the operator and security-contact placeholders in:

- `apps/evidence_lab/site/privacy.html`;
- `apps/evidence_lab/site/security.html`.

## Remaining operator responsibilities

- provide the Kubernetes cluster, registry, DNS and TLS credentials;
- verify the storage class is private and supports RWX;
- pin all image digests, including ClamAV;
- grant and audit the cluster-scoped admission-policy deployment permission;
- configure ingress/WAF logs without recording upload content;
- replace legal/operator placeholders in the Site policies;
- monitor ClamAV signature freshness;
- retain the successful validation output, admission self-test and deployment image digests.
