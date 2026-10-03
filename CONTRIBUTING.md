# Contributing

MoolBase is an experimental developer-preview reasoning substrate. Contributions
should preserve the evidence-first and safety-first posture. Historical
`GrapheneDB`, `graphene` and `GRAPHENEDB_*` implementation/API names remain
supported for compatibility.

## License and contribution provenance

MoolBase is distributed under the Apache License 2.0. By submitting a
contribution for inclusion in MoolBase, you agree that the contribution is
submitted under the repository's Apache-2.0 license, consistent with section 5
of that license.

This project also uses the Developer Certificate of Origin 1.1 (DCO) to record
that contributors have the right to submit their work under the project
license. The verbatim DCO is in `DCO-1.1.txt`.

Sign off each commit with:

```bash
git commit -s
```

which adds a line of the form:

```text
Signed-off-by: Your Name <you@example.com>
```

Do not submit proprietary, confidential, copied, generated, or third-party code
unless you have the right to contribute and redistribute it under the
applicable license. If a contribution incorporates third-party material,
identify its source and license in the PR and update
`THIRD_PARTY_NOTICES.md` when redistribution requires it.

The DCO is a certification of contribution provenance; it is not a copyright
assignment.

## Local validation before a PR

Run:

```bash
./scripts/run_all_tests.sh
./scripts/run_graphene_uniqueness_demo.sh
```

For a full release-style validation, run:

```bash
bash scripts/run_alpha_release_gate.sh
```

For storage/search changes, also run:

```bash
./scripts/run_sanitizers.sh
./scripts/run_crash_matrix.sh
```

For performance-sensitive changes, run:

```bash
./scripts/run_100k_stress.sh
```

## Contribution rules

- Do not weaken validation to make tests pass.
- Do not silently ignore WAL corruption.
- Do not add placeholder replay data.
- Do not allow vector dimension mismatches.
- Do not store invalid edges.
- Preserve provenance, contradiction visibility, abstention and no-silent-promotion invariants.
- Add tests for every new storage, recovery or epistemic-control behaviour.
- Update docs when changing user-visible API, durability or reasoning semantics.
- Preserve benchmark failures; do not remove or filter failing cases after a score-bearing run.
- Do not change a frozen benchmark protocol after score-bearing execution; open a new protocol version instead.
- Do not add vendored third-party source without license/provenance review.

## Coding style

- C++20.
- Prefer explicit `Status` returns for recoverable errors.
- Keep public API in `include/graphene`.
- Keep examples small and runnable.
