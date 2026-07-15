# GrapheneDB v0.6.0-rc1 Pilot Release Candidate

## Code Review and Validation Report

**Date:** 15 July 2026  
**Baseline:** `graphenedb_launch_hardened_source.zip`  
**Decision:** **GO for a controlled private/design-partner pilot. NO-GO for unrestricted public GA.**

## 1. Review objective

This iteration reviewed the launch-hardened server as an infrastructure product rather than adding more Kosh/reasoning features. The main objective was to close the gap between a hardened prototype and a usable pilot release contract:

- predictable process lifecycle;
- retry-safe writes;
- bounded and correctly classified HTTP requests;
- stable version/capability discovery;
- atomic bulk semantics;
- valid structured telemetry under concurrency and shutdown;
- a usable client contract;
- repeatable release validation and package-consumer proof.

The embedded C++ database remains authoritative. The HTTP server remains an optional controlled-pilot surface intended to run behind a TLS reverse proxy.

## 2. Important findings and fixes

### 2.1 Process termination was not a complete durability contract

**Finding:** The server could receive SIGTERM/SIGINT, but the lifecycle contract did not clearly stop admission, drain workers, checkpoint, emit machine-readable lifecycle evidence, and verify restart state.

**Fix:**

- close the listening socket from the signal handler to wake `accept()`;
- stop admission and drain the bounded worker pool;
- checkpoint by default before closing;
- support `--no-shutdown-checkpoint` only as an explicit operator choice;
- emit `shutdown_started`, `shutdown_checkpoint`, and `shutdown_complete` events;
- verify zero-byte WAL, restart reads, and post-restart validation in the contract test.

### 2.2 HTTP framing accepted too much ambiguity

**Finding:** A compact hand-written HTTP implementation must reject framing ambiguity rather than trying to be permissive.

**Fix:**

- reject malformed or conflicting `Content-Length` values;
- require `Content-Length` for JSON body endpoints;
- reject unsupported transfer encodings/chunked requests with HTTP 501;
- require `application/json` with HTTP 415 on mismatch;
- validate the request line and HTTP version;
- preserve request-size and socket-timeout limits.

This remains a compact origin server, not an internet edge HTTP stack.

### 2.3 Retried writes could create duplicates

**Finding:** Clients retry writes after timeouts or connection loss. Without a durable retry contract, an otherwise correct database can create duplicate memories.

**Fix:**

- add `Idempotency-Key` to `/v1/nodes` and `/v1/facts`;
- same key and same content returns the original ID with `idempotent_replay=true`;
- same key with conflicting content returns HTTP 409;
- serialize concurrent decisions for the same key;
- allow replay even when physical-lattice capacity has been reached;
- map genuinely new writes at capacity to HTTP 507.

### 2.4 Bulk ingestion was not an explicit atomic server contract

**Finding:** Per-item server loops risk partial writes, inconsistent failure behavior, and avoidable admission amplification.

**Fix:**

- use one core `put_batch()` operation;
- make the response declare `atomic=true`;
- add configurable `--max-bulk-nodes` with a hard upper clamp;
- limit generated bulk prefixes to 4 KiB;
- reject oversized bulk requests before mutation.

### 2.5 Several client errors surfaced as server failures

**Finding:** Invalid IDs, edges, hop counts, result sizes, and capacity conditions were not consistently bounded or mapped to appropriate HTTP status codes.

**Fix:**

- central status-to-HTTP mapping;
- invalid IDs and edges return HTTP 400;
- missing nodes return HTTP 404 where appropriate;
- capacity returns HTTP 507;
- node content limited to 1 MiB;
- lattice hops capped at 16;
- bundle hops capped at 8;
- search top-k capped at 1,000;
- temporal retrieval supports bounded limit/offset with a 1,000-result cap;
- integer parsing now rejects overflow.

### 2.6 Structured logs could become malformed during shutdown

**Finding discovered by the fresh release gate:** although request logging used a mutex, lifecycle events used separate streaming writes. A worker request log and shutdown event could interleave, producing a line that was not valid JSON. The first fresh CTest run caught this with a JSON decoder failure.

**Fix:**

- introduce one process-wide structured-log mutex;
- build every event as a complete string before emission;
- serialize request and lifecycle events through the same atomic emitter;
- run the pilot contract three consecutive times and then rerun all CTests.

This was a real correctness/operability defect found and fixed during this iteration.

### 2.7 No stable consumer-facing pilot contract

**Fix:**

- add public `GET /v1/version`;
- add `X-GrapheneDB-Version` and `X-GrapheneDB-API-Version` response headers;
- add OpenAPI 3.0.3 document;
- add a dependency-free Python client and runnable example;
- add `docs/PILOT_RELEASE_CONTRACT.md`;
- update repository agent instructions to match the current embedded-plus-optional-server boundary.

### 2.8 Default release tests were mixing smoke and target-scale campaigns

**Finding:** the default 5,000-incident RC stress profile could run beyond the ordinary CI window, obscuring failures in the rest of the suite.

**Fix:**

- make default CTest use a deterministic 1,000-incident stress smoke;
- preserve explicit 100k, 1m, and RC gate scripts as separate scale campaigns;
- add `scripts/run_pilot_rc1_gate.sh` as the repeatable pilot validation entry point.

This is test hygiene, not a claim that the larger performance concern is resolved.

### 2.9 Package installation included Python cache artifacts

**Finding:** package installation could create an empty `__pycache__` directory after client validation/import.

**Fix:** explicitly exclude `__pycache__` and `.pyc` from CMake installation and verify the installed tree.

## 3. Added pilot-facing artifacts

- `clients/python/graphenedb_client.py`
- `clients/python/README.md`
- `examples/python_client_demo.py`
- `docs/api/openapi-v1.yaml`
- `docs/PILOT_RELEASE_CONTRACT.md`
- `scripts/server_pilot_contract_test.py`
- `scripts/run_pilot_rc1_gate.sh`

## 4. Validation results

### 4.1 Clean release build

Environment:

- Linux x86_64 container
- CMake 3.31.6
- GCC/G++ 14.2.0
- Python 3.13.5
- Release configuration

Result: **PASS**.

### 4.2 Complete default test suite

Result: **29/29 tests passed**.

- total wall time: **13.45 seconds**;
- includes core API, C API, WAL/crash, fault injection, process kill, real filesystem failure, disk pressure, physical lattice, dense lattice, ACID lattice, extraction, package metadata, server launch hardening, pilot HTTP contract, Docker security static checks, and bounded stress/soak smoke;
- the server pilot contract was run three additional consecutive times after the atomic-log fix and passed each time.

Evidence: `reports/pilot-rc1/gate/ctest_all_after_log_fix.txt` and `server_pilot_contract_*.json`.

### 4.3 Pilot HTTP/client contract

**20/20 checks passed**, including:

- version discovery;
- Python client use;
- idempotent replay;
- idempotency conflict;
- Content-Length requirement;
- conflicting Content-Length rejection;
- chunked request rejection;
- JSON content-type requirement;
- atomic bounded bulk;
- bulk amplification guard;
- invalid-edge and invalid-node error mapping;
- bounded lattice and temporal retrieval;
- graceful shutdown events;
- checkpointed zero-byte WAL;
- restart durability and validation.

### 4.4 Realistic 30-second mixed server workload

Configuration:

- 6 clients;
- target 40 operations/second;
- physical lattice primary enabled.

Results:

| Measure | Result |
|---|---:|
| Writes | 634 |
| Reads | 223 |
| Vector searches | 197 |
| Lattice queries | 142 |
| Checkpoints | 3 |
| Total measured operations | 1,196 |
| Failures | **0** |
| p50 latency | **2.98 ms** |
| p95 latency | **4.75 ms** |
| p99 latency | **7.89 ms** |
| Maximum latency | **9.30 ms** |
| Restart sample reads | **4/4** |
| Post-restart validation | **OK** |
| Final WAL | **0 bytes** |

This is a controlled smoke/endurance result, not a substitute for the required 24-hour external soak.

### 4.5 Embedded 12,000-node stress profile

Configuration: 2,000 incidents, 12,000 nodes, 15,999 edges, 64-dimensional vectors, 100 queries, reopen enabled.

Results:

| Measure | Result |
|---|---:|
| Ingest throughput | **867 nodes/sec** |
| Vector p50 | **0.995 ms** |
| Vector p95 | **1.190 ms** |
| Vector p99 | **1.886 ms** |
| Causal p50 | **0.197 ms** |
| Causal p95 | **0.235 ms** |
| Causal hit rate | **0.99** |
| Reopen time | **480.8 ms** |

Important observation: `VectorIndexKind::Auto` resolved to the exact flat index at dimension 64 in this embedded profile. The pilot server explicitly uses KD-tree. The automatic policy and larger 5,000-incident profile remain performance work, not release-blocking correctness failures for the controlled pilot.

### 4.6 Package consumer

Result: **PASS**.

- clean install tree produced;
- downstream CMake consumer configured and linked against the installed package;
- package-consumer executable passed;
- no Python bytecode or `__pycache__` installed.

### 4.7 OpenAPI

OpenAPI 3.0.3 YAML parsed successfully and exposes 13 documented paths, including version discovery and node writes.

## 5. Pilot release decision

### Controlled private/design-partner pilot: GO

The current iteration is suitable for a controlled pilot where:

- the server runs behind an approved TLS reverse proxy;
- access is authenticated;
- operators monitor readiness/metrics and checkpoint behavior;
- database capacity is planned before deployment;
- backups and restore drills are part of the pilot operating procedure;
- pilot users accept RC format/API lifecycle constraints.

### Unrestricted public GA: NO-GO

The iteration does not prove:

- 24-hour or 72-hour endurance on intended production hardware/filesystem;
- actual OCI image vulnerability status or SBOM completeness;
- pinned and approved base-image digest supply chain;
- distributed replication, automatic failover, or multi-node consensus;
- tenant-level isolation or enterprise identity integration;
- stable migration compatibility across future durable format versions;
- internet-edge HTTP protocol completeness;
- acceptable runtime for the largest in-process 5,000-incident profile in this environment.

## 6. Remaining risks and next gates

1. **Run 24h, then 72h external soak.** Use `server_soak_test.py` on intended hardware. Track RSS, disk growth, checkpoint duration, p95/p99 drift, and restart validation.
2. **Resolve the higher-load long-soak harness issue.** Earlier 60-second high-rate harness attempts did not return cleanly even though server shutdown logs completed and WAL reached zero. Diagnose the harness/client termination path before using it as release evidence.
3. **Optimize or explicitly classify the 5,000-incident stress profile.** Determine whether flat-vector scan, reopen work, lattice rebuilding, or test construction dominates.
4. **Run actual container security gates.** Build the OCI image, produce SBOM, run Trivy/Grype, pin approved base image digest, and sign release artifacts.
5. **Replace the placeholder licence.** Public distribution must not use the current evaluation placeholder.
6. **Keep the server behind a reverse proxy.** The compact origin implementation is deliberately bounded but is not a full RFC-complete edge server.
7. **Define format migration policy.** Before a public stable release, document how storage-format version changes are rejected, upgraded, rolled back, and recovered.

## 7. Recommended next iteration

The next iteration should be **Pilot Operations RC2**, not another feature layer:

- external 24h/72h soak campaign;
- resource-leak and disk-growth telemetry;
- higher-load soak harness repair;
- automatic checkpoint policy validation under long load;
- actual OCI/SBOM/security scan;
- signed release manifest;
- durable-format compatibility/migration test fixture;
- one real incident-memory design-partner deployment.

Do not add dialectic/model-world/Kosh features to this branch until these operational gates pass.
