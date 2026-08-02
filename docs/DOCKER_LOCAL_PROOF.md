# Dockerised GrapheneDB and local proof harness

This package provides two distinct container experiences:

1. a hardened local GrapheneDB HTTP server built from the existing production-oriented `Dockerfile`;
2. a toolchain-complete proof appliance built from `Dockerfile.proof` that compiles and validates the exact source copied into the image.

The server and proof harness are deliberately separate. The runtime image stays small and non-root; the proof image contains compilers, Python, sanitizers and benchmark tooling.

## Prerequisites

- Docker Engine 24 or newer;
- Docker Compose v2;
- at least 8 GB RAM and 10 GB free disk for the full proof;
- Git, so the helper can stamp the exact source commit into the evidence bundle.

On Apple Silicon or another non-amd64 host, set:

```bash
export GRAPHENEDB_PLATFORM=linux/amd64
```

The current hardened runtime image copies amd64 dynamic libraries into a scratch image. The proof appliance itself is architecture-neutral where the Ubuntu toolchain is available.

## Fastest end-to-end demonstration

```bash
bash scripts/docker/local_stack.sh smoke
```

This command:

1. removes any previous local database volume;
2. builds the secure GrapheneDB server image;
3. starts the server on `127.0.0.1:8080`;
4. waits for `/v1/health`;
5. imports a five-node incident evidence graph;
6. invokes `/v1/reason/runtime` twice;
7. verifies the same target, FiberBundle hashes and Lyapunov energy are returned;
8. checks that all main runtime layers executed and no silent truth promotion occurred;
9. writes a signed evidence bundle to `docker-evidence/`.

Inspect the service:

```bash
docker compose -f docker-compose.local.yml ps
curl -s http://127.0.0.1:8080/v1/health \
  -H 'X-API-Key: local-development-key'
```

Stop it without deleting data:

```bash
bash scripts/docker/local_stack.sh down
```

## Proof modes

### Critical offline proof

```bash
bash scripts/docker/local_stack.sh proof-smoke
```

Runs:

- critical FiberBundle, lineage, epistemic-control, receipt and Lyapunov CTest contracts;
- the embedded HypoKosh demo;
- the real POSIX server contract;
- the 700-execution dialectic intervention suite;
- the committed offline cross-dataset structural gate.

### Complete alpha proof

```bash
bash scripts/docker/local_stack.sh proof-full
```

Runs the repository's complete alpha gate:

- clean Release configure/build;
- full CTest;
- installed-package consumer using `find_package(GrapheneDB)`;
- controlled dialectic intervention benchmark;
- offline structural benchmark;
- immutable manifest and checksums;
- separate server-enabled runtime contract.

### Memory-safety and fuzz proof

```bash
bash scripts/docker/local_stack.sh security
```

Runs:

- ASAN and UBSAN;
- TSAN;
- 10,000 LLVM libFuzzer executions against WAL input handling.

Some Docker hosts restrict ThreadSanitizer or low-level process behaviour. A platform restriction must be recorded as a blocker, not converted into a pass.

### Public 2,500-record structural run

```bash
bash scripts/docker/local_stack.sh public
```

Downloads and normalises the frozen public sample:

- 1,400 bAbI structures;
- 500 HotpotQA structures;
- 600 FEVER structures.

This measures the committed structural critic/control gates. It does not measure answer exact match, retrieval recall or generated semantic truth.

## Make targets

The same operations are exposed through `Makefile.docker`:

```bash
make -f Makefile.docker docker-smoke
make -f Makefile.docker docker-proof-smoke
make -f Makefile.docker docker-proof-full
make -f Makefile.docker docker-proof-security
make -f Makefile.docker docker-proof-public
```

## Evidence format

Every run creates:

```text
docker-evidence/
  <commit-prefix>-<mode>/
    environment.txt
    commands.log
    proof.log
    proof-manifest.json
    proof-summary.md
    checksums.sha256
    ...raw test and benchmark output...
  graphenedb-proof-<commit-prefix>-<mode>.tar.gz
```

Verify an evidence directory:

```bash
cd docker-evidence/<commit-prefix>-<mode>
sha256sum -c checksums.sha256
python3 -m json.tool proof-manifest.json
```

The manifest binds the run to `GRAPHENEDB_SOURCE_COMMIT`, records mode and environment, inventories every evidence file and carries the explicit claim boundary.

## Independent validation protocol

For a genuine independent reproduction:

1. use a fresh VM or developer machine;
2. clone the public or authorised private repository into a new directory;
3. detach at the exact commit being tested;
4. confirm `git status --porcelain` is empty;
5. run `proof-full` and `security`;
6. optionally run `public` where external network access is permitted;
7. give the `.tar.gz` evidence archives to another reviewer;
8. record tester name, date, host OS, Docker version and commit SHA.

A run performed by the implementation author is reproducible evidence, but it is not independent validation.

## Security boundary

The local server:

- binds only to `127.0.0.1` by default;
- runs as UID/GID 10001 inside the scratch image;
- uses a read-only root filesystem;
- drops all Linux capabilities;
- enables `no-new-privileges`;
- applies CPU, memory and PID limits;
- stores database state in a named volume.

The local API key defaults to `local-development-key` only in `docker-compose.local.yml`. Set a different value for shared machines:

```bash
export GRAPHENEDB_API_KEY="$(openssl rand -hex 32)"
```

The local Compose stack is not an internet-facing production deployment. Use the separate hardened proxy/deployment configuration for controlled pilots.

## What this proves

A passing proof bundle establishes that:

- the supplied source commit compiled in the declared container toolchain;
- the selected unit, integration, server and packaging contracts completed;
- the controlled intervention and structural gates reproduced;
- repeated runtime execution was deterministic for the seeded incident graph;
- raw outputs, environment and checksums were preserved.

It does not prove:

- semantic truth;
- universal answer accuracy;
- causal identification;
- global Lyapunov convergence;
- production availability or security;
- superiority over mature vector or graph databases;
- enterprise-GA readiness.

## Troubleshooting

### Output files are owned by root

The Compose proof services use `LOCAL_UID` and `LOCAL_GID`, populated by the helper. Run through `scripts/docker/local_stack.sh` rather than calling Compose manually.

### Build runs out of memory

```bash
export GRAPHENEDB_JOBS=1
export GRAPHENEDB_PROOF_MEMORY=10g
bash scripts/docker/local_stack.sh proof-full
```

### Public benchmark cannot download

Run `proof-smoke` or `proof-full`; both use the committed offline fixture. The public mode is an additional external-data gate.

### Port 8080 is occupied

```bash
export GRAPHENEDB_PORT=18080
bash scripts/docker/local_stack.sh smoke
```
