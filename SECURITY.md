# Security policy

## Supported status

MoolBase v0.6.0-alpha.2 is a released developer preview for evaluation and controlled pilots. It is not an enterprise GA security-certified product.

## Security boundary

MoolBase (with existing GrapheneDB API/package compatibility names) is primarily an embedded library and local CLI. It also ships an
optional controlled-pilot HTTP origin server on supported POSIX platforms. The
server provides a single API-key boundary, bounded workers/queues, request
limits, and rate limiting, but it is not an internet-edge server, enterprise
identity provider, multi-tenant authorization layer, or encryption-at-rest
system.

Embedded callers are responsible for:

- operating-system access control on database directories
- process/user isolation for tenants
- filesystem or volume encryption when data at rest must be encrypted
- secret management outside MoolBase/GrapheneDB metadata/content fields

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

Please do not disclose an unpatched vulnerability in a public issue. First use GitHub's private **Report a vulnerability** flow from this repository's Security tab when it is available. If that private flow is unavailable, contact the project owner through an existing non-public channel before sharing exploit details. Include affected versions, impact, reproduction steps and any proposed mitigation. If you cannot establish a private channel, do not post technical exploit details publicly.

## Distribution and supply-chain policy

- Source and binary distributions are licensed under Apache-2.0 and must include
  the repository `LICENSE` and `NOTICE` files.
- Third-party redistribution obligations must be recorded in
  `THIRD_PARTY_NOTICES.md`.
- Public release artifacts must include a source-commit-bound manifest and
  SHA-256 checksum.
- Release artifacts produced by GitHub Actions should carry a GitHub/Sigstore
  build-provenance attestation when the platform supports it.
- A release is not represented as enterprise GA merely because it has a signed
  or attested artifact.

## Required security checks before a GA claim

- Multi-hour coverage-guided fuzzing.
- ASAN/UBSAN/TSAN clean runs.
- Disk-pressure crash tests.
- Path traversal review.
- Dependency/license review.
- Release artifact checksum/manifest verification.
- SBOM and dependency-vulnerability review for the exact release configuration.
- Signed/attested release provenance verification.
