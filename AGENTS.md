# MoolBase / GrapheneDB Agent Context

MoolBase v0.6.0-alpha.2 is a released developer preview of a C++20 embedded database and optional controlled-pilot HTTP server for causal/lattice AI memory. The durable core stores nodes, vectors, metadata, snapshots, WAL/checkpoint state, typed edges, and physical hex-lattice coordinates. The server adds bounded concurrency, authenticated API access, readiness/metrics, retry-safe writes, checkpointing, backup, and a versioned pilot API.

The project is **not** a distributed database, SQL engine, internet edge proxy, general vector-database replacement, or material-science simulator. Keep the embedded library authoritative. The compact HTTP server is an optional product surface and must remain behind a TLS reverse proxy for non-loopback deployment.

MoolBase is the public product name; `GrapheneDB`, `graphene` and `GRAPHENEDB_*` remain implementation/API names. For newcomer integrations, use [Agent integration](docs/AGENT_INTEGRATION.md) and the documented `v0.6.0-alpha.2` checkout. Do not assume the default branch contains customer examples. The repository is public and `LICENSE` contains Apache 2.0.

## Start Here

Read before broad changes:

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/STORAGE_FORMAT.md`
- `docs/PILOT_RELEASE_CONTRACT.md`
- `docs/api/openapi-v1.yaml`
- `reports/pilot-rc1/PILOT_RC1_CODE_REVIEW_AND_VALIDATION.md` when present

Domain references:

- Lattice: `docs/GRAPHENE_LATTICE_MODEL.md`, `docs/LATTICE_RETRIEVAL.md`
- Packaging: `docs/PACKAGING_DISTRIBUTION.md`
- Security: `SECURITY.md`, `deploy/`, `Dockerfile`, `docker-compose.secure.yml`
- Release workflows: `scripts/run_pilot_rc1_gate.sh`, `scripts/verify_package_install.sh`

## Project Layout

- `include/graphene/` — public C/C++ API.
- `src/` — storage and retrieval implementation.
- `tools/graphenedb_cli.cpp` — embedded CLI.
- `tools/graphenedb_server.cpp` — optional controlled-pilot HTTP server.
- `clients/python/` — dependency-free Python pilot client.
- `tests/` — core, durability, crash, and lattice tests.
- `bench/` — explicit benchmark executables.
- `scripts/` — release, stress, package, server, and evidence workflows.
- `reports/` — generated validation evidence; avoid committing scratch output.

## Non-Negotiable Engineering Rules

- Preserve durable-format compatibility. Any durable record change requires a format-version decision, migration/recovery tests, and `docs/STORAGE_FORMAT.md` updates.
- Keep the embedded library authoritative; server endpoints must call tested core APIs rather than duplicate storage logic.
- Writes exposed over HTTP must be retry-safe or explicitly document why not. Preserve `Idempotency-Key` behavior for `/v1/nodes` and `/v1/facts`.
- Keep bulk ingestion atomic and bounded. Never reintroduce partial per-item bulk writes.
- Keep the worker pool and request queue bounded. Do not use detached per-connection threads.
- Do not weaken request-size, hop, result-count, rate-limit, bind-address, or reverse-proxy controls without evidence.
- Do not silently promote inferred/reinforced data into observed/discovered truth.
- Keep JSON logs and API responses valid for arbitrary user-controlled text.
- Treat graceful shutdown, checkpointing, restart, backup/restore, and second-open rejection as database correctness contracts.
- Public claims must match evidence. `v0.6.0-alpha.2` is a released developer preview for evaluation and controlled pilots, not enterprise GA.
- Do not add Kosh/dialectic/model-world features to the DB correctness branch unless the user explicitly reopens that scope.

## Verification

Fast pilot contract:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON -DGRAPHENEDB_BUILD_SERVER=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
python3 scripts/server_pilot_contract_test.py ./build/graphenedb_server
```

Repeatable pilot RC gate:

```bash
bash scripts/run_pilot_rc1_gate.sh
```

Explicit larger profiles remain separate from default CTest:

```bash
bash scripts/run_100k_stress.sh
bash scripts/run_1m_stress.sh
bash scripts/run_rc_gate_pack.sh
```

Package-consumer verification:

```bash
bash scripts/verify_package_install.sh
```

## Current maintenance scope

Use the released `v0.6.0-alpha.2` tag for newcomer examples. Follow the user's task; do not default to another release-preparation or research programme.

For documentation changes, check links, commands and the diff. Preserve release tags, assets, checksums, historical reports and frozen scientific contracts. Run implementation tests when the implementation changes.

Long hardware soak, target-scale testing and security hardening are separate future production-readiness work; they do not reopen the completed developer-preview release.
