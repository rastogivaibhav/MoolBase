# GrapheneDB Launch Hardening Review and Validation

## Scope

This iteration reviewed the launch-candidate source and addressed the identified server and operational blockers while keeping the core embedded database API stable.

## Executive result

The controlled-pilot launch blockers listed for this iteration are implemented and covered by automated checks:

- bounded worker pool and bounded admission queue
- overload rejection
- per-client token-bucket rate limiting
- structured JSON request logs and Prometheus telemetry
- secure non-loopback bind policy and reverse-proxy/TLS deployment contract
- hardened non-root container specification and static security validation
- reusable mixed-operation soak test
- adverse-filesystem/fault/process-kill test wrapper
- physical-radius capacity formula, startup gate, runtime endpoint, and readiness protection

A further reliability defect was found during review: a reset client could trigger `SIGPIPE` while the server wrote a response. The server now ignores `SIGPIPE`, and a reset-connection survival test passes.

This is suitable for a controlled developer or design-partner pilot behind a trusted TLS reverse proxy. It is not yet an unconditional public-GA declaration because the 24-hour approved-host soak and a real built-image CVE/SBOM gate still need to be executed in an environment with Docker/Podman and an approved scanner.

## Implemented changes

### 1. Bounded concurrency and overload behaviour

The detached-thread-per-connection path was replaced with `BoundedWorkerPool`:

- fixed worker count (`--workers`)
- bounded queue (`--queue-capacity`)
- immediate HTTP 503 with `server_overloaded` when admission is full
- queue depth and rejection telemetry
- joined worker shutdown

### 2. Rate limiting and request safeguards

- token-bucket rate limiting per effective client IP
- configurable rate and burst
- HTTP 429 plus `Retry-After`
- request-size ceiling with HTTP 413
- socket read/write timeouts with HTTP 408
- constant-time API-key comparison
- API key may be supplied through `GRAPHENEDB_API_KEY`
- public liveness/readiness endpoints reveal no database counts

### 3. Structured telemetry

- one JSON log object per request with timestamp, request ID, client IP, method, path, status, duration, byte counts, and rate-limit state
- log writes serialized to avoid interleaving under concurrency
- JSON metrics endpoint
- Prometheus exposition endpoint
- counters for active/completed/rejected/rate-limited/auth-failed requests, status classes, bytes, durations, operations, checkpoints, backups, and validation

### 4. TLS/reverse-proxy boundary

The internal server remains plain HTTP by design. The boundary is now explicit:

- non-loopback bind is rejected without an API key
- non-loopback bind is rejected unless a TLS proxy is acknowledged, unless an explicit insecure override is supplied
- optional enforcement of `X-Forwarded-Proto: https`
- proxy headers are trusted only when explicitly enabled
- Caddy and NGINX deployment guidance added
- secure Compose keeps the GrapheneDB port unpublished
- the Caddy service now receives `GRAPHENEDB_HOST` explicitly

### 5. Container hardening

- multi-stage Debian slim build
- non-root UID/GID 10001
- no secret baked into the image
- read-only-root-compatible runtime layout
- healthcheck utility
- stack protector, FORTIFY, PIE, RELRO, and BIND_NOW
- secure Compose controls: capability drop, `no-new-privileges`, PID/memory/CPU limits, read-only root filesystem
- static Docker/Compose validator
- release-environment container gate script requiring a real image build and Trivy or Grype scan

### 6. Soak and realistic usage testing

The reusable soak harness performs concurrent mixed operations:

- writes
- point reads
- vector/hybrid searches
- lattice-neighbour queries
- periodic checkpoints
- validation
- final checkpoint
- restart sample reads
- post-restart validation

The same harness accepts `--seconds 86400` for the formal 24-hour run.

### 7. Adverse-filesystem testing

The launch wrapper runs:

- real filesystem failure tests
- disk-pressure tests
- DB fault injection
- process-kill matrix

When run as root, filesystem tests are executed as an unprivileged user so permission failures are meaningful.

### 8. Physical-radius capacity planning

For a one-layer axial hex disk:

```text
capacity(R) = 1 + 3R(R + 1)
R_min(N) = ceil((-3 + sqrt(12N - 3)) / 6)
```

Implemented controls:

- `--expected-max-nodes`
- startup rejection when configured radius is undersized
- authenticated `/v1/admin/capacity`
- readiness failure when capacity is exhausted
- documented 80% alert and 90% migration thresholds

Examples:

| Target | Minimum radius | Recommended |
|---:|---:|---:|
| 10,000 | 58 | 64 |
| 100,000 | 183 | 200 |
| 1,000,000 | 577 | 600 |
| 10,000,000 | 1,826 | 1,900 |

## Validation evidence

### Targeted CTest

Five selected gates passed:

- core DB tests
- physical-lattice-primary tests
- RC5 fault injection
- server launch hardening
- Docker security static validation

Result: **5/5 passed**.

### Server launch-hardening suite

All checks passed, including:

- secure public-bind rejection
- physical-capacity preflight rejection
- readiness
- authentication
- capacity endpoint
- Prometheus endpoint
- request-size rejection
- client-reset/SIGPIPE survival
- writes, validation, checkpoint
- structured logs
- restart persistence
- rate limiting
- bounded-queue overload rejection

### 180-second mixed server soak

Configuration:

- 12 concurrent clients
- target 120 operations/second
- 180 seconds

Results:

- 11,796 writes
- 4,311 point reads
- 3,138 searches
- 2,196 lattice queries
- 3 periodic checkpoints plus final checkpoint
- 21,441 measured requests
- failures: **0**
- p50: **5.74 ms**
- p95: **12.42 ms**
- p99: **28.25 ms**
- max: **243.91 ms**
- queue rejections: **0** during normal-load soak
- rate-limit responses: **0** during normal-load soak
- final WAL: **0 bytes**
- restart sample reads: **4/4 passed**
- post-restart DB validation: **OK**

### Adverse-filesystem suite

All passed:

- real filesystem failures
- disk pressure
- fault injection
- process-kill matrix

### Compiler and binary hardening

- strict server/healthcheck build passed with `-Wall -Wextra -Wpedantic -Werror`
- hardened server is PIE
- GNU RELRO present
- BIND_NOW present

### Docker structural checks

All static checks passed:

- multi-stage image
- non-root user
- no baked API key
- hardened compiler/linker flags
- healthcheck
- minimal apt usage
- read-only Compose root filesystem
- dropped capabilities
- no-new-privileges
- PID limit
- DB port not published
- Caddy hostname environment wiring
- build outputs excluded from context

## Remaining release-environment evidence gates

The following are deliberately not represented as completed here:

1. **24-hour approved-host soak**
   - Run `SOAK_SECONDS=86400 scripts/run_launch_readiness_gate.sh` on the intended filesystem and hardware profile.

2. **Actual OCI image build and current CVE/SBOM inspection**
   - Run `scripts/run_container_security_gate.sh` where Docker and Trivy or Grype are installed.
   - Pin organisation-approved base-image digests before release.

3. **Native TLS**
   - Not implemented; the supported architecture is private HTTP behind a trusted TLS reverse proxy.

4. **Public distribution licence**
   - `LICENSE` is still a placeholder and must be replaced before a public release.

5. **HTTP protocol breadth**
   - The server is a compact HTTP/1.1 implementation for controlled pilots, not a general-purpose edge HTTP stack. Public exposure must remain behind the reverse proxy.

## Launch decision

- Embedded DB correctness work: **unchanged and retained**
- Controlled private pilot: **GO**, subject to reverse-proxy and API-key deployment controls
- Public internet-facing GA: **NO-GO until the 24-hour soak, actual image scan/SBOM, immutable image pinning, and licence decision are complete**
