# GrapheneDB Evidence Lab deployment

This deployment keeps ChatGPT Sites as the public user interface and exposes only the Evidence Lab gateway. The GrapheneDB pilot server is never internet-facing.

## Security architecture

- uploads are extension, signature, archive, credential and ClamAV scanned before parsing;
- the gateway has no GrapheneDB binary in the production profile;
- every run creates one restricted Kubernetes Job;
- the worker has no service-account token;
- a deny-all NetworkPolicy blocks worker ingress and egress;
- the worker root filesystem is read-only and all Linux capabilities are dropped;
- CPU, memory, ephemeral-storage and wall-clock limits are applied;
- the job and run directory are deleted after result collection;
- anonymous sessions expire after one hour and support immediate deletion.

## Prerequisites

- Kubernetes 1.27 or newer with Pod Security Admission;
- an ingress controller and TLS issuer;
- a `ReadWriteMany` storage class for the private gateway/worker exchange PVC;
- a container registry accessible by the cluster;
- DNS for the public gateway hostname;
- the final ChatGPT Sites origin URL.

The committed manifest uses `evidence-lab.example.com`; replace it before applying.

## Build images

Run from the repository root:

```bash
docker build \
  -f apps/evidence_lab/gateway/Dockerfile.production \
  -t ghcr.io/rastogivaibhav/graphenedb-evidence-lab-gateway:0.2.0 .

docker build \
  -f apps/evidence_lab/worker/Dockerfile \
  -t ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker:0.2.0 .

docker push ghcr.io/rastogivaibhav/graphenedb-evidence-lab-gateway:0.2.0
docker push ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker:0.2.0
```

For public launch, replace mutable tags with image digests in the deployment evidence.

## Create deployment configuration

```bash
cp deploy/evidence-lab/kubernetes/evidence-lab-secrets.example.yaml \
   /tmp/evidence-lab-secrets.yaml
```

Edit the copy and replace:

- ChatGPT Sites origin;
- public gateway hostname;
- worker image tag or digest;
- exact verified GrapheneDB commit;
- public release version.

Do not commit the populated secret.

The ClamAV manifest uses the official stable image tag for initial deployment. Resolve and pin its image digest before the public launch gate.

## Apply

```bash
kubectl apply -f deploy/evidence-lab/kubernetes/evidence-lab.yaml
kubectl apply -f /tmp/evidence-lab-secrets.yaml
kubectl -n graphenedb-evidence-lab rollout status deployment/evidence-lab-clamav
kubectl -n graphenedb-evidence-lab rollout status deployment/evidence-lab-gateway
```

Check:

```bash
kubectl -n graphenedb-evidence-lab get pods,job,svc,ingress
curl -fsS https://evidence-lab.example.com/v1/public/health | python -m json.tool
```

## Run the public live gate

```bash
python -m pip install 'httpx>=0.27,<1'
python scripts/validate_evidence_lab_public.py \
  https://evidence-lab.example.com \
  --expected-commit "$(git rev-parse HEAD)"
```

A release is not valid unless the command executes:

- two sample packs;
- one uploaded structured dataset;
- live GrapheneDB receipts;
- three downloadable reproduction bundles with valid checksums;
- immediate session deletion.

## Connect ChatGPT Sites

Set the Site's server-side backend base URL to the gateway HTTPS origin. The browser must never receive a permanent gateway token. Add the published Site origin to `EVIDENCE_LAB_ALLOWED_ORIGINS` and the gateway hostname to `EVIDENCE_LAB_ALLOWED_HOSTS`.

The reference static frontend reads:

```javascript
window.GRAPHENEDB_EVIDENCE_LAB_API
```

Use the same public API contract from ChatGPT Sites.

## Remaining operator responsibilities

- provide the Kubernetes cluster, registry, DNS and TLS credentials;
- verify the storage class is private and supports RWX;
- pin all image digests;
- configure ingress/WAF logs without recording upload content;
- publish privacy, retention and prohibited-data notices;
- monitor ClamAV signature freshness;
- retain the successful validation output and deployment image digests.
