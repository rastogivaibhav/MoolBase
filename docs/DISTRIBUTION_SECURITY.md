# Distribution security and license verification

GrapheneDB source and official project-built binary distributions are licensed
under the Apache License, Version 2.0.

## Files that must ship

A distributable package is rejected by the repository release tooling unless it
contains:

- `LICENSE` — canonical Apache-2.0 license text;
- `NOTICE` — GrapheneDB copyright/attribution notice;
- `THIRD_PARTY_NOTICES.md` — third-party redistribution boundary;
- a release manifest bound to the source commit;
- a SHA-256 checksum.

Official GitHub Actions distribution runs additionally generate an SPDX 2.3
SBOM. Non-PR distribution runs generate GitHub/Sigstore provenance and SBOM
attestations when GitHub artifact attestations are available.

## Verify the checksum

For a downloaded release bundle:

```bash
sha256sum -c graphenedb-<version>-linux-x86_64.zip.sha256
```

The digest must also match `package_sha256` in the adjacent
`*.manifest.json`.

## Verify source provenance

The release manifest contains an exact `source_commit`. Confirm it exists in
this repository and that the version fields match the intended release.

For an attested artifact, GitHub CLI can verify provenance:

```bash
gh attestation verify graphenedb-<version>-linux-x86_64.zip \
  -R rastogivaibhav/MoolBase
```

## Verify the SBOM attestation

Official distribution runs generate an SPDX JSON SBOM and, outside pull
requests, attest it against the release artifact.

```bash
gh attestation verify graphenedb-<version>-linux-x86_64.zip \
  -R rastogivaibhav/MoolBase \
  --predicate-type https://spdx.dev/Document/v2.3
```

## Optional FAISS builds

The default GrapheneDB distribution does not bundle FAISS. If
`GRAPHENEDB_USE_FAISS=ON` is used, FAISS is supplied separately by the build
environment. Redistributors of a FAISS-linked package must include the
applicable FAISS and transitive notices from the exact FAISS distribution they
ship.

## Contribution provenance

GrapheneDB uses Apache-2.0 plus Developer Certificate of Origin 1.1 sign-off for
incoming contributions. See `CONTRIBUTING.md` and `DCO-1.1.txt`.

## Release status is not a security certification

Checksums, SBOMs and attestations establish integrity/provenance of an artifact.
They do not mean GrapheneDB is enterprise GA, formally verified, or suitable
for high-stakes deployment. See `SECURITY.md` for the current boundary.
