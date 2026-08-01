# GrapheneDB arXiv paper package

## Manuscript

`main.tex` is the proposed technical preprint:

> **GrapheneDB: Lineage-Aware Epistemic Control and Frontier-Aware Evidence Recovery for Agentic Systems**

The manuscript narrows the earlier TheHypoKosh and Dialectical Model World whitepapers to claims supported by the merged GrapheneDB C++20 implementation and controlled experiments.

## Complete package

- `main.tex` — full submission manuscript.
- `references.bib` — primary research references.
- `arxiv_metadata.json` — copy-ready arXiv metadata with explicit human placeholders.
- `CLAIM_EVIDENCE_MATRIX.md` — claim audit and prohibited overclaims.
- `ARTIFACT_MANIFEST.md` — paper-to-code evidence map.
- `REPRODUCIBILITY.md` — frozen-release and experiment protocol.
- `RELATED_SYSTEMS_POSITIONING.md` — category comparison and benchmark boundaries.
- `PRE_SUBMISSION_REVIEW.md` — final gap review and blockers.
- `ARXIV_SUBMISSION_CHECKLIST.md` — operational submission gates.
- `Makefile` — build, validate, and package commands.
- `../scripts/check_arxiv_package.py` — metadata, citation, filename, and claim-boundary validator.
- `../scripts/package_arxiv_source.sh` — clean deterministic arXiv source packager.

## Build and validate

```bash
make -C paper check
```

This validates the metadata and citations, compiles the PDF with `latexmk`, and runs a non-blocking `chktex` pass.

## Create the arXiv source archive

```bash
make -C paper arxiv
```

Outputs:

```text
paper/build/arxiv/graphenedb-arxiv-source.tar.gz
paper/build/arxiv/graphenedb-arxiv-source.sha256
```

The archive contains only:

```text
main.tex
references.bib
main.bbl
MANIFEST.sha256
```

## Strict final-submission validation

After all human placeholders and public release identifiers are replaced:

```bash
python3 scripts/check_arxiv_package.py --paper-dir paper --strict
```

## Supported thesis

> An embedded evidence database can make source ancestry, evidence-family independence, derivation lineage, contradiction and missing-path progress first-class runtime state, allowing an agent to distinguish sufficient grounds for action from cases that require targeted recovery, contestation or abstention.

The paper does not claim AGI, causal identification, semantic truth, global convergence, million-node production readiness, enterprise GA, or general superiority over mature graph/vector databases and agent systems.

## Relationship to earlier papers

The earlier documents remain architectural history and future research direction. They introduced premature convergence, temporal-causal memory, FiberBundles, opposition, model-world feedback, a small-model role, and a route toward model-world learning. The submission paper changes several points to match the actual code and evidence:

- C++20 GrapheneDB replaces the earlier Rust-first implementation description.
- Conditional targeted recovery replaces always-on recursive self-improvement.
- Frontier progress is separated from completed-bundle change.
- Opposition is operational only when enabled and supplied with reopen targets.
- Full FiberBundles are ephemeral; compact receipts are durable output.
- AGI, million-node model-world, official external-baseline, and implementation-feedback claims are excluded from current results.

## Merge versus submit

This package can be merged as working research documentation. It must remain a draft submission until the legal/IP, code licence, frozen public release, exact-head validation, identity/affiliation, metadata, endorsement, and external-review gates are complete.
