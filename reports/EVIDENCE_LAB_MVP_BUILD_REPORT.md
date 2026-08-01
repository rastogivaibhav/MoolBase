# GrapheneDB Evidence Lab — implementation, hardening and deployment report

Date: 2026-08-01  
Branch: `feature/public-evidence-lab-mvp`  
Status: deployment-ready code; public infrastructure and exact-head live evidence pending

## Scope implemented

### Developer experience

- FastAPI public gateway under `/v1/public`;
- anonymous expiring sessions and delete-now;
- inspectable and downloadable CC0 sample library;
- JSON, NDJSON/JSONL, canonical ZIP and multi-file CSV/TSV ingestion;
- canonical reference validation, graph-size limits and deterministic dataset hashes;
- recorded-reference backend with explicit non-live labelling;
- live GrapheneDB adapter using atomic `/v1/extractions` and `/v1/reason/runtime`;
- evidence graph, governed status, metrics, compact receipt and execution events;
- responsive frontend suitable as the ChatGPT Sites reference implementation;
- public privacy and security templates.

### Reproducibility

Every run bundle now contains:

- the exact original uploaded files or published sample files under `source/`;
- the exact normalised GrapheneDB dataset;
- execution configuration, events and raw engine result;
- public result, evidence graph and compact receipt;
- worker image reference and exact source commit;
- executable POSIX gateway replay;
- PowerShell replay guidance;
- a SHA-256 inventory covering every bundled file.

The public validator rejects bundles without source inputs, replay scripts, complete checksums or matching run/commit identity. A public Kubernetes run must identify a worker image by immutable `@sha256:` digest.

### Upload hardening

- extension and content-signature checks;
- UTF-8/binary rejection for structured text formats;
- path traversal and ZIP Slip rejection;
- symlink and encrypted-member rejection;
- nested archive rejection;
- archive member count, member size, uncompressed-size and compression-ratio limits;
- allowlisted canonical archive filenames and duplicate-basename rejection;
- executable-like or ambiguous archive extras rejected;
- credential-pattern scanning for private keys and common cloud/developer tokens;
- optional or mandatory ClamAV INSTREAM scanning before parsing;
- duplicate upload filename rejection;
- request body limits before complete buffering.

### Gateway hardening

- public Host and CORS allowlists;
- approved cross-origin API responses for ChatGPT Sites;
- Kubernetes health-probe compatibility without disabling public Host validation;
- request identifiers;
- secure response headers and no-store caching;
- per-client and anonymous-session sliding-window rate limits;
- sanitised backend errors in public mode;
- cross-session dataset, run and bundle isolation tests;
- non-root gateway container with read-only deployment filesystem.

### Worker isolation

- resource-limited local subprocess mode for exact-head validation;
- minimal environment and private temporary workspace;
- process group termination and workspace deletion;
- address-space, CPU, open-file, process-count, output-file and core-dump limits;
- self-contained worker binary build with missing-link check;
- Kubernetes one-Job-per-run production backend;
- no worker service-account token;
- deny-all worker ingress and egress;
- non-root, read-only root filesystem, RuntimeDefault seccomp and all capabilities dropped;
- CPU, memory, ephemeral-storage and wall-clock limits;
- private image-pull credentials;
- bounded retry for shared-storage output visibility;
- automatic Job, process and workspace deletion.

### Retention

- anonymous sessions expire after one hour;
- delete-now removes the session tree immediately;
- a restricted Kubernetes CronJob independently removes expired sessions every ten minutes;
- cleanup does not require a Kubernetes API token and uses the same private PVC.

### Deployment automation

- exact-source gateway and worker Dockerfiles;
- Kubernetes namespace, restricted Pod Security labels, RBAC, private RWX PVC, ClamAV, gateway Service, TLS Ingress, NetworkPolicy and cleanup CronJob;
- deployment preflight validator;
- guarded GitHub Actions deployment workflow;
- private GHCR pull-secret creation;
- gateway, worker and ClamAV deployment by immutable image digest;
- deployment source commit forced to equal the workflow checkout commit;
- mandatory post-deployment live validation;
- captured deployment evidence artifact;
- ChatGPT Sites API connection guide.

## Validation completed before final hardening

Environment:

- Linux x86-64;
- Python 3.13;
- FastAPI 0.128.2;
- Pydantic 2.13.4;
- GNU C++ 14.2.0 for the available local GrapheneDB source snapshot.

Passed:

1. Original gateway suite — 4/4 tests.
2. Python compilation for the original gateway modules.
3. JavaScript syntax validation.
4. Both sample `SHA256SUMS` inventories.
5. Deterministic sample loading and hashing.
6. Recorded-mode HTTP smoke:
   - health;
   - session creation;
   - sample listing;
   - upload and validation;
   - sample and uploaded-dataset runs;
   - recorded/live boundary fields;
   - graph, metrics and compact receipt;
   - reproduction-bundle generation.
7. Build of `graphenedb_server` from the available local source snapshot.
8. Focused upload-security checks for clean structured data, traversal, ZIP Slip, nested archives and credential leakage.

The expanded latest-head tests, exact live integration and container builds are present in CI but have not executed because the GitHub Actions runner fails before checkout.

## Live integration findings

### Atomic extraction contract

The first live gateway run reached a real GrapheneDB server and initially failed during relation ingestion because the pilot extraction API resolves `external_id` references atomically within one extraction request. Sending nodes first and relations in separate source batches returned:

```text
relation endpoint external_id not found
```

The adapter now submits the complete canonical node-and-relation graph in one atomic `/v1/extractions` request. Per-edge public source, evidence-family and derivation lineage remains in relation metadata.

### Local source snapshot limitation

The only full GrapheneDB source archive available in the local execution environment was older than the merged runtime endpoint. It built `graphenedb_server` and accepted atomic extraction, but returned `404` for `/v1/reason/runtime`.

That result does not indicate a failure in the current GitHub implementation. It establishes that the old archive cannot be used as evidence for the exact current head.

## Exact-head validation and runner status

The branch contains two independent exact-head gates:

```bash
bash scripts/run_evidence_lab_live_gate.sh
```

and the `exact-head-live-integration` GitHub Actions job.

Each gate builds `graphenedb_server` from the checked-out branch and requires:

- two live sample runs;
- one live uploaded-dataset run;
- `live=true` and `run_mode=live_graphenedb`;
- receipt confirmation that GrapheneDB executed;
- exact source-commit identity;
- immutable worker image identity in the public profile;
- original source inputs inside all three bundles;
- complete SHA-256 inventories and replay scripts;
- immediate anonymous-session deletion and inaccessible artifacts afterwards.

GitHub Actions Evidence Lab run `30692296363` failed before checkout. Gateway job `91349043063` recorded no steps; dependent live-integration and container jobs were skipped. An earlier failed run was explicitly retried and again returned zero steps and no log blob. This is a runner/account infrastructure failure, not a failing test assertion or compiler diagnostic.

No exact-head live PASS is claimed until the local gate or a functioning CI runner completes successfully.

## Public deployment boundary

The production deployment package is implemented but has not been applied to a real public cluster in this work session. No public endpoint, TLS certificate, DNS record or ChatGPT Sites backend connection is claimed.

Deployment requires operator-provided values that cannot be invented or embedded in source:

- protected kubeconfig for the target cluster;
- GHCR package-read credentials;
- public gateway hostname and DNS;
- TLS issuer/certificate;
- published ChatGPT Sites HTTPS origin;
- private RWX storage class;
- operator legal name and security contact for the policy pages.

The guarded deployment workflow refuses placeholders, builds immutable images, deploys the hardened profile and runs the public live gate before it can be treated as successful.

## Remaining release gates

1. Restore GitHub Actions execution or run the exact-head live gate on an approved Linux host.
2. Obtain one green latest-head gateway/security test run, container build and exact live integration.
3. Provision the protected `evidence-lab-production` GitHub environment and required secrets.
4. Confirm private RWX storage, restricted Pod Security, ingress and TLS support on the target cluster.
5. Replace public privacy/security operator placeholders.
6. Run the guarded deployment workflow with the real hostname and ChatGPT Sites origin.
7. Preserve successful live-validation output, image digests and deployment evidence.
8. Connect ChatGPT Sites to the validated HTTPS gateway and test from a logged-out external browser.

These external gates are tracked in issue #14.

## Honest claim boundary

This work provides a sandboxed public developer alpha implementation and deployment package. It is not a hosted production database, semantic truth engine, regulated-data service or enterprise-GA system.
