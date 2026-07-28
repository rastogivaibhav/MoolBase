# Remote source status

Branch: `codex/generic-data-tokenized`

The branch contains:

- the latest generic/tokenised reasoning implementation as a complete unified source patch;
- frozen metrics and validation report;
- a run-oriented `README.md`;
- repository-wide `AGENTS.md`;
- `CODEX.md` and `CLAUDE.md` handoff instructions.

## Verified local source

The tested source snapshot was configured and built in Release mode with tests enabled and benchmarks disabled. The complete CTest run passed 45/45 tests. The focused recursive-model-world suite covers generic structured predicates and token-tagged data while asserting:

- exact final answer;
- ordered semantic reasoning path;
- non-zero HypoKosh rounds;
- Graphene ingestion/model-world execution;
- dialectic expansion, opposition and convergence;
- governed projection and no-silent-promotion behaviour.

## Materialisation gate

The remote branch currently carries the latest implementation through `GRAPHENEDB_GENERIC_DATA_TOKENIZED.patch`; it is not yet a fully materialised replacement of every source file from the tested local snapshot. Apply and review the patch against the preceding canonical-relation source line, then run CI on the resulting tree before merging.

Do not describe this branch as a complete full-tree push or merge it as production-ready until that materialisation and remote CI gate are complete.
