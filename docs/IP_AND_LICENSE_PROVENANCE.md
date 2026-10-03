# IP and license provenance

This document records the repository-level provenance checks required before a
MoolBase public source or binary release is represented as Apache-2.0
licensed. Historical GrapheneDB identifiers remain part of the compatibility
and research lineage.

It is an engineering/release control, not legal advice.

## Automated repository findings

As of the distribution-hardening review:

- the visible Git commit history contains 135 commits;
- every commit author identity observed belongs to the repository maintainer
  (GitHub `rastogivaibhav` / Vaibhav Rastogi);
- no outside commit author was found in the visible history;
- repository code search found no GPL, MIT, SPDX, proprietary "all rights
  reserved", or other conflicting source-file license headers;
- the default core build does not vendor a third-party runtime source library;
- optional FAISS is externally supplied and documented separately;
- the top-level `LICENSE` is canonical Apache License 2.0 text and is pinned
  by the distribution validator;
- project attribution is kept in `NOTICE`.

These checks establish repository consistency. They do not establish that a
commit author is legally the copyright owner.

## Maintainer rights-holder sign-off

Before the first hardened public release/tag is created, the maintainer must
confirm all of the following:

- [ ] I have the right to license the MoolBase code and documentation under
      Apache-2.0.
- [ ] No material part of the repository is confidential or proprietary code
      belonging to an employer, client, previous employer, contractor,
      collaborator, or other third party unless I have written permission to
      redistribute it under compatible terms.
- [ ] Any code or text copied or adapted from a third-party source is
      identified and its license obligations are recorded.
- [ ] AI-assisted code was generated from prompts/context I was entitled to
      use and was not knowingly copied from an incompatible source.
- [ ] Any future human contributor will contribute under Apache-2.0 and the
      project's DCO 1.1 process.
- [ ] If my rights position changes, I will stop distribution until the
      licensing boundary is corrected.

Record the sign-off as a dated comment in the governing rights-provenance
GitHub issue. Do not put confidential employment or client agreements in the
public repository.

## Future contribution rule

The release maintainer should review contribution provenance before merging
outside code. A Git commit's DCO sign-off records the contributor's
certification but does not replace third-party-license review where the
contribution incorporates external material.

## Release gate

A hardened public release should require all of the following:

1. the rights-holder sign-off above has been recorded;
2. Distribution integrity CI passes on the release source;
3. protected branch/tag governance is enabled;
4. release package contains LICENSE, NOTICE and third-party notices;
5. package checksum, source-bound manifest and SPDX SBOM are published;
6. provenance/SBOM attestations are generated and independently verifiable.
