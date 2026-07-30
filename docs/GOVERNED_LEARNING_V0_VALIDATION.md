# GDB-GL-0 Governed Learning Validation

## Result

Date: 2026-07-24

`GDB-GL-0` is implemented and passes its controlled-pilot mechanism gates.
This result demonstrates governed retrieval-policy learning. It does not
demonstrate autonomous LLM-weight training, longitudinal improvement, PayPal
scale, cryptographic tenant isolation, or the preregistered `G8`/`G9`
research claims in `deepmindtest.md`.

## Evidence

| Gate | Result |
|---|---|
| Versioned specification validator | Passed, 22 requirements |
| Core HypoKosh/outcome-learning contract | Passed |
| Authenticated HTTP/restart contract | Passed |
| Complete release CTest suite | Passed, 37/37 |
| Installed CMake package consumer | Passed |
| OpenAPI/server route parity | Passed, 32 paths |
| Python client syntax/import compilation | Passed |
| Hardened production OCI build | Passed |
| Production container identity | `10001:10001` |
| Production container root filesystem | Read-only |
| Production container capabilities | All dropped |
| Production `no-new-privileges` | Enabled |
| Production learning loop | Passed |
| Production graceful restart and policy recovery | Passed |
| `git diff --check` | Passed |

The production loop used seven verified episodes across training, development,
and evaluation splits. It excluded the evaluation episode from selection,
recommended `candidate-v2`, rejected an unapproved promotion, promoted the
candidate after explicit approval, used the active policy in a read-only
HypoKosh query, appended a rollback to `baseline-v1`, checkpointed on restart,
and recovered `baseline-v1`.

## Requirement audit

- `GL-001` through `GL-003`: the core and HTTP contracts bound proposals,
  preserve hypothetical origin, and assert no node/edge writes.
- `GL-004` through `GL-010`: tests cover invalid dimension, invalid counts,
  non-finite metrics, exact replay, changed-replay conflict, split eligibility,
  safety utility `-1`, and deterministic bounded utility/classification.
- `GL-011` and `GL-012`: tests cover minimum samples, improvement, a
  safety-negative veto, a worst-domain-regression veto, held-out exclusion,
  and no evaluation writes.
- `GL-013` through `GL-016`: tests cover rejected unapproved activation,
  approved promotion, idempotent decision replay, changed-replay conflict,
  append-only rollback, current-policy recovery, legal hold, and quarantine.
- `GL-017` and `GL-018`: tests cover training-export exclusions and separately
  reported data-policy metrics.
- `GL-019`: strict authenticated routes are represented in OpenAPI and the
  dependency-free Python client; unknown fields are rejected.
- `GL-020`: the full existing suite and package consumer pass. Learning uses
  ordinary storage-v2 nodes and extraction-v1 identity, so no durable format
  version changed.
- `GL-021`: the actual production image completed the loop as a non-root user
  with a read-only root filesystem and survived restart.
- `GL-022`: the specification and release documentation explicitly exclude
  self-training, calibrated-probability, tenant-isolation, distributed, and
  autonomous-action claims.

## Commands

```bash
cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=ON \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build /build -j2
ctest --test-dir /build --output-on-failure -j2

bash scripts/verify_package_install.sh
docker build -t graphenedb-server:governed-learning .
python3 scripts/remote_governed_learning_demo.py \
  --base-url http://127.0.0.1:18091 \
  --api-key '<redacted>'
```

The temporary production-validation container and its disposable volume were
removed after the restart check. The image remains local under
`graphenedb-server:governed-learning`.

## Remaining gates

Before broader announcement or enterprise use:

1. Run the preregistered blinded longitudinal experiment with at least 1,000
   verified training episodes and 300 held-out episodes, confidence intervals,
   ablations, and external reproduction.
2. Build an identity-aware control plane that binds authenticated principals
   to tenant IDs. The pilot API key alone is not tenant isolation.
3. Add an offline trainer/optimizer service if model-weight or learned-ranker
   training is desired. GrapheneDB currently exports eligibility and policy
   recommendations; it does not train weights.
4. Validate sharding, admission control, retention, legal deletion, backup,
   and disaster recovery under the intended hundreds-of-agents workload. The
   current server is a bounded single-node pilot, not a distributed database.
5. Complete 24-hour and 72-hour soak gates on the target filesystem and
   hardware.
6. Pin approved base-image digests, generate an SBOM, and run current
   Trivy/Grype scans. The validation build succeeded but is not a CVE
   attestation.
7. Replace the placeholder licence before public distribution.
