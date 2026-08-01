# GrapheneDB Evidence Lab — implementation, security and deployment report

Date: 2026-08-01  
Source branch: `feature/public-evidence-lab-mvp`  
Frozen validation branch: `release/evidence-lab-alpha.1-rc3`  
Status: deployment-candidate code; exact-head live and public-cluster evidence pending

## Product scope implemented

The Evidence Lab provides:

- a responsive ChatGPT Sites reference frontend;
- anonymous, one-hour sessions with immediate delete-now;
- inspectable and downloadable CC0 sample packs;
- JSON, JSONL/NDJSON, canonical ZIP and multi-file CSV/TSV ingestion;
- canonical graph validation, lineage preservation and deterministic dataset hashes;
- explicit recorded-reference mode that never claims GrapheneDB executed;
- live GrapheneDB ingestion through `/v1/extractions` and reasoning through `/v1/reason/runtime`;
- governed status, evidence graph, execution metrics, events and compact epistemic receipts;
- complete reproduction bundles with source input, canonical graph, raw result, receipt, replay scripts and SHA-256 inventory.

## Upload and gateway security

Uploads are screened before parsing for:

- unsupported extensions, binary content and signature mismatches;
- path traversal, ZIP Slip, symlinks and encrypted archive members;
- nested archives, duplicate basenames and executable-like extras;
- excessive member count, member size, expansion size and compression ratio;
- private keys and common cloud/developer credential patterns;
- malware through ClamAV INSTREAM in public mode.

The gateway implements Host and CORS allowlists, request-size controls, request identifiers, no-store and browser-security headers, per-client/session rate limits, sanitised public errors and cross-session dataset/run/bundle isolation.

## Worker isolation

Local validation uses a resource-limited subprocess with process-group termination and private temporary storage.

Public execution uses one Kubernetes Job per run with:

- a tokenless worker service account;
- an immutable worker image digest and fixed Python entrypoint;
- no init or ephemeral containers;
- no lifecycle, probe, `envFrom`, device or port hooks;
- a fixed nine-variable environment contract;
- non-root UID/GID 10001;
- read-only root filesystem;
- all Linux capabilities dropped;
- RuntimeDefault seccomp;
- no host namespaces, alternate runtime, priority class or custom node scheduling;
- only the private Evidence Lab PVC and memory-backed `/tmp`;
- deny-all worker ingress and egress;
- CPU, memory, ephemeral-storage and wall-clock bounds;
- one pod, one attempt and bounded Job retention;
- automatic Job and workspace deletion.

## Gateway Job-creation admission guard

The gateway must create Kubernetes Jobs, so RBAC alone would allow a compromised gateway to submit a different workload. RC3 adds a cluster `ValidatingAdmissionPolicy` and binding that constrain gateway-created Jobs to the exact worker contract.

The deployment gate:

1. requires Kubernetes 1.30 or newer;
2. installs the policy with `failurePolicy: Fail` and `Deny`/`Audit` actions;
3. waits for `status.typeChecking`;
4. rejects every CEL expression warning;
5. uses the gateway's actual `create` permission to dry-run an exact worker;
6. modifies that manifest to use the gateway service account and confirms the request is denied by the Evidence Lab policy.

Required markers:

```text
EVIDENCE_LAB_ADMISSION_POLICY_TYPECHECK=PASS
EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS
```

## Namespace resource guardrails

The Kubernetes profile includes:

- a `LimitRange` bounding each container to 1 CPU, 2 GiB memory and 512 MiB ephemeral storage;
- a `ResourceQuota` bounding aggregate CPU, memory, pods, Jobs, CronJobs, PVCs and requested storage;
- restricted Pod Security Admission labels;
- a private RWX PVC;
- immutable gateway, worker and ClamAV image digests;
- a ten-minute cleanup CronJob for expired sessions.

## Reproducibility contract

Every completed run ZIP includes:

```text
source/
normalised/dataset.json
execution/configuration.json
execution/events.json
execution/raw-engine-result.json
result/public-result.json
result/evidence-graph.json
result/compact-receipt.json
reproduce.sh
reproduce.ps1
README.md
SHA256SUMS
```

The public validator requires two live sample runs, one live uploaded-dataset run, exact source identity, immutable worker identity, three valid bundles, complete checksums and post-delete inaccessibility.

## Validation completed

Completed locally before the final cluster-only controls:

- original gateway suite: 4/4 tests;
- Python and JavaScript syntax checks;
- sample checksum verification;
- deterministic sample loading and hashing;
- recorded-mode HTTP and bundle flow;
- focused scanner tests for clean input, traversal, ZIP Slip, nested archives and credential leakage;
- build of the available local GrapheneDB server snapshot;
- correction of the atomic extraction contract after a real server integration attempt.

The repository now also contains generated-worker contract tests, admission-policy script syntax checks, exact-head live gates, container builds and post-deployment validation. These latest gates have not executed on GitHub-hosted runners because the repository's Actions jobs repeatedly terminate before checkout with zero steps and no log artifact.

## Live integration finding

The first real-server run exposed that external node IDs used by relations must be resolved atomically in one extraction request. The adapter now submits the complete canonical node-and-relation graph in one `/v1/extractions` transaction while preserving source, evidence-family and derivation lineage in relation metadata.

The local source archive available after that correction predates `/v1/reason/runtime`; it cannot serve as exact-current-head evidence.

## Deployment automation

The guarded deployment workflow:

1. verifies the expected commit equals the workflow checkout;
2. builds and pushes gateway and worker images;
3. resolves immutable gateway, worker and ClamAV digests;
4. verifies Kubernetes 1.30+;
5. renders and dry-runs the namespace, quota, admission and application resources;
6. installs the admission policy and namespace guardrails;
7. creates private-registry and application secrets;
8. proves the positive and negative worker admission cases;
9. deploys ClamAV, gateway and cleanup components;
10. runs the public HTTPS validation gate;
11. preserves rendered resources, image identities, policy status, quotas and cluster events.

## Current blockers

No public endpoint is claimed. Completion requires operator-controlled inputs:

- a Kubernetes 1.30+ cluster and protected kubeconfig;
- permission to install the cluster-scoped policy and impersonate the gateway service account for its self-test;
- private RWX storage;
- GHCR package-read credentials;
- public DNS and TLS;
- the published ChatGPT Sites HTTPS origin;
- approved operator, privacy and security contact text;
- public code-licence and sample-rights confirmation.

The exact frozen RC must then produce:

```text
EVIDENCE_LAB_HOST_PREFLIGHT=PASS
EVIDENCE_LAB_EXACT_HEAD_LIVE_GATE=PASS
EVIDENCE_LAB_ADMISSION_POLICY_TYPECHECK=PASS
EVIDENCE_LAB_WORKER_ADMISSION_POLICY=PASS
EVIDENCE_LAB_LIVE_VALIDATION=PASS
```

## Claim boundary

This is a sandboxed public developer-alpha implementation and guarded deployment package. It is not a hosted production database, regulated-data service, semantic truth engine or enterprise-GA system.
