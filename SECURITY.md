# Security policy

## Supported status

This repository is a v1 RC / controlled pilot candidate. It is not yet an enterprise GA security-certified product.

## Security boundary

GrapheneDB is primarily an embedded library and local CLI. It also ships an
optional controlled-pilot HTTP origin server on supported POSIX platforms. The
server provides a single API-key boundary, bounded workers/queues, request
limits, and rate limiting, but it is not an internet-edge server, enterprise
identity provider, multi-tenant authorization layer, or encryption-at-rest
system.

Embedded callers are responsible for:

- operating-system access control on database directories
- process/user isolation for tenants
- filesystem or volume encryption when data at rest must be encrypted
- secret management outside GrapheneDB metadata/content fields

The optional server must use an API key and an approved TLS reverse proxy for
non-loopback deployment. Its administrative endpoints currently share the same
API-key boundary as data endpoints, so operators must restrict server access to
trusted pilot clients. A hosted or multi-tenant distribution must add separate
administrative authorization, tenant isolation, audit policy, and encryption
controls before being represented as enterprise GA.

## Security-sensitive areas

- WAL parsing and replay.
- Framed file checksum validation.
- Lock-file handling.
- Path handling for backup/open/compact.
- Fuzz inputs under `fuzz/`.
- Metadata parsing in Kosh adapter TSV ingest.

## Reporting issues

For now, report security issues privately to the project owner before public disclosure.

## Required security checks before public release

- Multi-hour coverage-guided fuzzing.
- ASAN/UBSAN/TSAN clean runs.
- Disk-pressure crash tests.
- Path traversal review.
- Dependency/license review.
- Final public license selection.
- Release artifact checksum manifest review.
