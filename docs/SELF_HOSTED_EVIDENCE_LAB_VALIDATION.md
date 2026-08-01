# Self-hosted GrapheneDB Evidence Lab validation

This path exists because GitHub-hosted jobs for this repository have repeatedly failed before checkout with zero recorded steps and no log artifact. The self-hosted gate does not relax the release criteria.

## Trust model

Use a dedicated, disposable or tightly controlled Linux x86-64 machine. Do not place the runner on the public Kubernetes cluster control plane or on a machine containing unrelated production secrets.

The validation job executes repository code and builds containers. Treat repository write access and workflow approval as privileged capabilities.

## Minimum host

- Linux x86-64;
- 4 GiB RAM minimum, 8 GiB recommended;
- 10 GiB free disk minimum, 25 GiB recommended;
- Git;
- Bash;
- CMake;
- Ninja;
- C++20-capable compiler;
- Python 3.11+ with `venv`;
- Node.js;
- curl;
- Docker with access for the runner user;
- outbound access to GitHub and Python package indexes.

Verify the host from a checkout:

```bash
bash scripts/preflight_evidence_lab_validation_host.sh
```

Expected final marker:

```text
EVIDENCE_LAB_HOST_PREFLIGHT=PASS
```

## Register the runner

In GitHub:

1. Open repository **Settings → Actions → Runners**.
2. Add a new self-hosted runner.
3. Follow GitHub's generated Linux x64 installation commands on the dedicated host.
4. Add this custom label during runner configuration:

```text
graphenedb
```

The final label set must include:

```text
self-hosted
linux
x64
graphenedb
```

Do not commit the short-lived registration token or runner credentials.

Install the runner as a service only on a dedicated machine. For a one-time disposable host, interactive execution is acceptable and easier to destroy afterwards.

## Execute the frozen release candidate

Use branch:

```text
release/evidence-lab-alpha.1-rc3
```

From GitHub Actions, run:

```text
Evidence Lab Self-Hosted Gate
```

Supply the exact `expected_commit` shown on the frozen RC branch. The workflow refuses a mismatch between the input, checkout and `GITHUB_SHA`.

The workflow performs:

1. clean checkout;
2. host preflight;
3. isolated Python environment creation;
4. gateway, upload-security, retention and cross-session tests;
5. generated Kubernetes worker-contract tests;
6. sample checksum and source syntax validation;
7. exact-head GrapheneDB server build;
8. two live sample runs and one live uploaded-dataset run;
9. exact commit and compact receipt verification;
10. reproduction-bundle source and SHA-256 validation;
11. production gateway and worker image builds;
12. gateway-container smoke test;
13. evidence artifact generation;
14. generated workspace cleanup.

Required self-hosted success markers include:

```text
EVIDENCE_LAB_HOST_PREFLIGHT=PASS
EVIDENCE_LAB_EXACT_HEAD_LIVE_GATE=PASS
```

Download and retain the workflow artifact named:

```text
evidence-lab-self-hosted-<commit>
```

## Direct execution without a registered runner

A trusted engineer may execute the exact gate from a fresh clone:

```bash
git clone --branch release/evidence-lab-alpha.1-rc3 --single-branch \
  https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1

git status --short
git rev-parse HEAD
bash scripts/preflight_evidence_lab_validation_host.sh
bash scripts/run_evidence_lab_live_gate.sh 2>&1 | tee evidence-lab-live-gate.log
grep -F 'EVIDENCE_LAB_EXACT_HEAD_LIVE_GATE=PASS' evidence-lab-live-gate.log

docker build -f apps/evidence_lab/gateway/Dockerfile.production \
  -t evidence-lab-gateway:rc3 .
docker build -f apps/evidence_lab/worker/Dockerfile \
  -t evidence-lab-worker:rc3 .
```

Record:

- `git rev-parse HEAD`;
- complete live-gate log;
- gateway and worker image inspection output;
- host OS, compiler, CMake, Python and Docker versions;
- SHA-256 values for all evidence files.

A direct local PASS can unblock technical review, but the public deployment must still pass the post-deployment HTTPS validator and the Kubernetes admission-policy self-test.

## Cluster-side admission gate

The public cluster must be Kubernetes 1.30 or newer. The guarded deployment installs a `ValidatingAdmissionPolicy` that restricts the gateway service account to the exact immutable, tokenless GrapheneDB worker Job shape.

After the immutable worker image is available, run:

```bash
bash scripts/validate_evidence_lab_admission_policy.sh \
  'ghcr.io/rastogivaibhav/graphenedb-evidence-lab-worker@sha256:FULL_DIGEST'
```

Required marker:

```text
EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS
```

This proves both sides of the control: the exact worker is accepted and an arbitrary gateway-created Job is denied.

## After a successful run

1. Attach or link the evidence artifact in PR #13.
2. Keep PR #13 draft until the exact RC3 live gate passes.
3. After the live PASS, mark PR #13 ready for security and deployment review.
4. Do not merge solely because the local gate passed; review the admission, security and deployment diff.
5. Provision the production Kubernetes and DNS values tracked in issue #14.
6. Deploy only from the same frozen commit or create a new RC and repeat the full gate.
7. Run the admission-policy self-test and public HTTPS validator after deployment.
8. Complete the ChatGPT Sites private-preview and logged-out browser gates.

## Runner cleanup

After release validation:

- remove the self-hosted runner from repository settings when it is not needed continuously;
- delete runner registration credentials from the machine;
- remove build images and temporary volumes;
- destroy a disposable validation VM;
- retain only the approved validation artifact and checksums.
