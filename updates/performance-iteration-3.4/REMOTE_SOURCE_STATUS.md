# PI3.4 Remote Source Status

Status: **canonical source validated; remote tree reconciliation not yet materialised in full**.

## Canonical source

Validated bundle:

`graphenedb_performance_iteration_3_4_source.zip`

SHA-256:

`f9837126bb92ded1a39ead678267b0eab5cc6cfcde41a7567abd075ce03b40e9`

The bundle was re-executed locally on 2026-08-19 and passed the permanent PI3.4 gate, the complete 66/66 product regression suite, GCC ThreadSanitizer with `halt_on_error=1`, and Clang 17 ASAN+UBSAN.

## Remote-master anchor

The retained `graphenedb_v1-master.zip` snapshot from 2026-08-17 was compared with the current remote baseline. Key Git blob IDs match the current repository exactly:

- `CMakeLists.txt`: `2be6768368d0228753ef4d3754628b109d0855d0`
- `src/db.cpp`: `11ec1f3f230a6ad3aebb240ec56f3e7faabed454`
- `include/graphene/types.hpp`: `185ba14204b4eb4939c1c71a7610973fefc0913a`
- `include/graphene/db.hpp`: `b7df21ed998bb1d95ae355cbd821fa53dfbe4f07`

This makes the reconciliation analysis a comparison against the actual remote-master-era source line rather than an inferred baseline.

## Reconciliation size

Remote master -> accepted PI3.4 canonical source:

- 222 files added
- 90 files modified
- 114 files removed
- 423 files unchanged
- Git patch summary: 352 changed paths, 18,562 insertions, 6,376 deletions

The full binary-capable reconciliation patch generated from those two exact snapshots has SHA-256:

`eff7b98b93db407bc2674181ae322fa5edd2050e3a1c3808baac256f0613ce7d`

Compressed patch SHA-256:

`44f5d1a75e17a6841d33d2e621487df331d5cb264c4103463a602f8b42c68d1f`

## Why PR #20 remains draft

The GitHub connector available in this execution environment can create/update individual files and Git trees, but it cannot apply the generated local patch or upload a local directory as one atomic commit. Materialising only the PI3.4 benchmark files would create a non-canonical hybrid because accepted PI3.4 depends on the later Engineering Baseline Core/Reasoning split, temporal/activation primitives, release governance, test isolation, and performance changes that are not yet present on remote `master`.

Therefore PR #20 must not be represented as the complete accepted PI3.4 tree until the reconciliation patch is materialised or the full canonical source snapshot is committed through a capable Git client.

## Authority boundary

Even after materialisation, PI3.4 approves the int16 semantic shadow only for controlled experimental integration. It remains disabled/non-authoritative, exact-float verified, and is not approved for `VectorIndexKind::Auto`, CLI default, server default, or answer authority.
