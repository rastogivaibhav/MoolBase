# MoolBase technology transfer and independent evaluation — v1 candidate

**Audience:** external research and platform engineers. **Status:** developer-preview evaluation; no institutional endorsement, production certification or comparative superiority claim. This package adds review tooling only. Source baseline: `9a6f8266e650867133d9fc9b424923274adcbc6c` (master at preparation); the evaluation checkout commit and binaries are recorded by the runner. The published audit.3 evaluation distribution is separately pinned to `f78640d11bc204a8571c1c31e58b6a0f4464880a`; do not assume byte or result parity between them. The older quickstart/integration guide still names audit.2. For this package use the source checkout and commands below, not mixed binary/example releases.

## 1. Independent start

On an isolated Linux x86_64 host: Git, C++20 compiler (GCC 13 or equivalent), CMake >=3.25, Python >=3.10, adequate disk and CPU. No model credentials or paid compute for the acceptance run.

```sh
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
git checkout --detach <reviewed-transfer-commit>
python3 evaluation/transfer/acceptance.py --out /tmp/moolbase-transfer-evidence --jobs 2
```

Replace the placeholder with the full reviewed commit SHA from the transfer PR. Use a new output directory outside the source tree; do not reuse data. The runner records git SHA, compiler/tool versions, individual exit codes, wall time, logs, binary SHA-256 and a file manifest. A failing test is not hidden; the process exits nonzero. No tests are executed against a pre-existing service. Existing upstream tests remain authoritative.

For a package-only install see [published installation instructions](https://moolbase.rasvai.com/INSTALL.html); obtain matching database and examples artifacts and check the published SHA-256 manifests before extraction. For audit.3 use its published pin and matching packages. Checksums are integrity evidence, not publisher authentication.

## 2. Architecture and scientific claims

| Layer | Actual implementation anchor | Hypothesis to challenge |
|---|---|---|
| Durable evidence | `include/graphene/db.hpp`, storage implementation, WAL/checkpoint | Atomic ingest, reopen, validation and backup preserve committed evidence |
| Hex lattice | [lattice model](../../docs/GRAPHENE_LATTICE_MODEL.md), [retrieval](../../docs/LATTICE_RETRIEVAL.md) | Axial topology improves useful traversal or resource cost over ordinary adjacency under matched budgets |
| FiberBundle | `src/fiber_bundle.cpp` | Preserve competing paths and lineage; copies do not count as independent corroboration |
| HypoKosh | `src/hypokosh_runtime.cpp` | Governed hypothesis competition improves decision quality over fixed ranking |
| Opposition and belief revision | `src/model_world.cpp`, runtime and control | Change or reopen commitments under material opposition rather than overwrite |
| Stability critic | `src/stability_critic.cpp`, Lyapunov critic tests | Detect false convergence/oscillation without treating stability as semantic truth |
| Receipt | `src/epistemic_receipt.cpp` | Reproducible evidence-to-decision provenance with bounded uncertainty |

Read [architecture](../../docs/ARCHITECTURE.md), [storage format](../../docs/STORAGE_FORMAT.md), [reasoning modes](../../docs/REASONING_MODES_AND_RECEIPTS.md) and [capabilities](../../docs/CAPABILITIES.md). A lattice is a data topology, not a physical simulation. A FiberBundle is a deterministic projection, not independent proof. Evidence roles, source families and semantic certificates are supplied by the caller. A resolved status means the configured policy was met, not objective truth. The runtime is bounded; a Lyapunov-style critic is not a proof of global convergence or correctness. Mathematical novelty and causal claims require separate review and controls; this document does not claim them.

## 3. API and integration

- Embedded C++: [customer engine](../../examples/customer_showcase/engine.cpp), public headers in `include/graphene/`; installed CMake target `GrapheneDB::graphenedb`. Lifecycle and prior-state persistence are application responsibilities.
- Pilot HTTP: [OpenAPI v1](../../docs/api/openapi-v1.yaml), [pilot contract](../../docs/PILOT_RELEASE_CONTRACT.md), [Python client](../../clients/python/), [HTTP runtime example](../../examples/customer_showcase/python_http.py).
- API inventory gap: `/v1/reason/runtime` appears in the executable example but not in the checked OpenAPI path list at the baseline. Treat OpenAPI as incomplete until route/schema conformance review. Do not confuse `/v1/reason/hypokosh` proposals with full runtime decisions. No published MCP server is asserted.
- Inspect API response status and alternatives, truncation, receipts and uncertainty. Never use a selected node alone as an answer. HTTP requests create fresh runtime state; evidence survives restart but prior decisions require application storage.

The acceptance runner builds and executes the native showcase lifecycle and malformed-input cases, runs the HTTP example and pilot contract, tests package consumption and runs CTest. Use the output receipts and logs to reproduce failures. Tests exercise bounded fixtures, not complete endpoint coverage.

For integration with Qwen or another agent, keep model-specific prompts outside the database: model proposes typed observations; independent verifier and source resolver check exact text, offsets, hashes and roles; application ingests through the existing contract; runtime returns governed result; application persists the receipt and refuses unsupported commitments. No model integration is claimed validated here.

## 4. Deployment and security

Native Linux evaluation is the default. Container starting point: [Dockerfile](../../Dockerfile), [secure Compose](../../docker-compose.secure.yml) and [security policy](../../SECURITY.md). Use the same reviewed source revision, build an image with `--build-arg SOURCE_COMMIT=$(git rev-parse HEAD)`, record image digest, SBOM and dependency scan. Base image pin alone does not make all build dependencies reproducible.

For a cloud research pilot (Alibaba ACK or another Kubernetes platform): one isolated instance, one encrypted persistent volume, one writer, private networking, an identity-aware TLS gateway, separate operator/admin access, scoped service identity and secret injection. No publicly exposed origin or shared cross-tenant DB. Use a single-replica workload, prevent overlapping writers during replacement, mount writable data only, use resource limits, readiness/liveness and non-root/no-capabilities controls. Verify restore from backup on an isolated instance before using any real data. Do not put credentials or restricted data in evidence logs. Existing server key is shared by admin and data routes; gateway policy and per-instance isolation are essential. Kubernetes deployment manifests and cloud validation remain outstanding; do not treat this guidance as tested IaC.

Production acceptance backlog:
1. Threat model with independent authz and tenant-isolation tests; separate admin plane; encryption and credential rotation.
2. Pinned image, signed provenance, SBOM/vulnerability scans, ASAN/UBSAN/TSAN and multi-hour fuzzing; remediate findings.
3. Document RPO/RTO, destructive backup/restore and disk-pressure recovery; monitor resources and error budgets.
4. Measure target workload throughput, p50/p95/p99 latency, storage growth, memory and concurrency with raw data.
5. Independently evaluate workload accuracy and safety; external operator signs acceptance. No HA, horizontal scale or enterprise GA claim before these pass.

## 5. Research evidence register

Historical evidence is deliberately separated from newly executed acceptance results. Read the [EGPM ledger at 173427c](https://github.com/rastogivaibhav/MoolBase/blob/173427c25457e61b3d12f3e42e66cc5559e4ed6d/benchmarks/egpm01/GATE_LEDGER.md) and original run artifacts; numbers below are ledger/log-reported, not independently recomputed by this package.

| Finding | Scope and limitation |
|---|---|
| [EGPM03](https://github.com/rastogivaibhav/MoolBase/actions/runs/45883634716): native semantic recall@20 65.25%, BM25 62.30%, hybrid 68.23% | 1,000 questions, 5,296 docs; retrieval metric, not QA superiority |
| [EGPM06](https://github.com/rastogivaibhav/MoolBase/actions/runs/45905423044): 21 admitted edges, 11 clearly supported, 3 ambiguous, 7 unsupported as labelled | repeated edges; nonblind manual review, not independent precision |
| [EGPM07](https://github.com/rastogivaibhav/MoolBase/actions/runs/45908047493): MeMo 2/8 EM, graph 0/8, oracle text 4/8 | development-only; earlier graph integration bypassed full native reasoning |
| [EGPM09](https://github.com/rastogivaibhav/MoolBase/actions/runs/45953171901): 10 lifecycle replays, 7 malformed inputs, HTTP replay/restart | bounded engineering checks |
| [EGPM10](https://github.com/rastogivaibhav/MoolBase/actions/runs/45958591956): 8 provisionally resolved annotated chains, 8 evidence-required missing-hop controls, 16 restart hashes | supplied gold answer/dependency projection; no independent QA or semantic truth claim |
| [EGPM12](https://github.com/rastogivaibhav/MoolBase/actions/runs/46008459005): previously reported 251 proposed, 8 refuting admitted, 0 supporting | reported from logs; raw artifact audit outstanding; verifier semantics unresolved |

Neither standalone MoolBase competitiveness against MeMo nor the combined system's superiority is established. No held-out gate closure, ontology benefit or component causal benefit asserted. Review source spans, model identities and artifact checksums before relying on historical results.

## 6. Independent evaluation framework

See [protocol](PROTOCOL.md) and `score.py --help`. The scorer is executable for reviewer-supplied predictions; it does not generate predictions or run MeMo. Existing EGPM inference drivers are separate experimental code, not validated standalone adapters. The six existing EGPM gates and frozen split are not replaced.

## 7. External acceptance record

External reviewer should record source SHA, execution environment, acceptance manifest hash, pass/fail per step, unsupported API routes, benchmark inputs and model hashes, deviations, negative results, security findings and decision: reject / revise / controlled pilot / production review. Sign independently; package author must not sign on reviewer’s behalf.

Known gaps: full OpenAPI conformance, cloud deployment execution, independent semantic adjudication, comparative inference runners and blind held-out evidence. This candidate enables local technical acceptance and common scoring; it is not a complete independently certified transfer.
