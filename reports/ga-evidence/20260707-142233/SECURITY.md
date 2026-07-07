# Security policy

## Supported status

This repository is a v1 RC / controlled pilot candidate. It is not yet an enterprise GA security-certified product.

## Security boundary

GrapheneDB is currently an embedded library and local CLI. It does not provide a network listener, remote authentication service, multi-tenant authorization layer, or encryption-at-rest. Callers are responsible for:

- operating-system access control on database directories
- process/user isolation for tenants
- filesystem or volume encryption when data at rest must be encrypted
- secret management outside GrapheneDB metadata/content fields

This boundary is intentional for the current controlled-pilot profile. A hosted or multi-tenant distribution must add authentication, authorization, audit logging, and encryption controls before being represented as enterprise GA.

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
