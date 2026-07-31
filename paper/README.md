# GrapheneDB arXiv paper package

## Manuscript

`main.tex` is the proposed technical preprint:

> **GrapheneDB: Lineage-Aware Epistemic Control and Frontier-Aware Evidence Recovery for Agentic Systems**

The manuscript intentionally narrows the earlier TheHypoKosh and Dialectical Model World whitepapers to the claims supported by the merged GrapheneDB C++20 implementation and controlled experiments.

## Package contents

- `main.tex` — submission manuscript.
- `references.bib` — primary paper references.
- `CLAIM_EVIDENCE_MATRIX.md` — claim audit and prohibited overclaims.
- `REPRODUCIBILITY.md` — frozen-release and experiment protocol.
- `RELATED_SYSTEMS_POSITIONING.md` — honest category comparison and benchmark plan.
- `ARXIV_SUBMISSION_CHECKLIST.md` — technical, release and human/legal gates.

## Build

```bash
cd paper
pdflatex -interaction=nonstopmode -halt-on-error main.tex
bibtex main
pdflatex -interaction=nonstopmode -halt-on-error main.tex
pdflatex -interaction=nonstopmode -halt-on-error main.tex
```

## Paper thesis

The paper does not claim that GrapheneDB is AGI, a causal discovery oracle, a semantic truth engine, or generally superior to mature graph/vector databases or external agent systems.

The supported thesis is:

> An embedded evidence database can make source ancestry, evidence-family independence, derivation lineage, contradiction and missing-path progress first-class runtime state, allowing an agent to distinguish sufficient grounds for action from cases that require targeted recovery, contestation or abstention.

## Relationship to earlier source papers

The earlier documents remain useful as architectural history and future research direction. They describe premature convergence, temporal-causal memory, FiberBundles, opposition, model-world feedback and long-term discovery ambitions. The arXiv manuscript changes several points to match the actual submitted implementation:

- C++20 GrapheneDB replaces the earlier Rust-first implementation description.
- Conditional targeted recovery replaces always-on recursive self-improvement.
- Frontier progress is separated from completed-bundle change.
- Opposition is operational only when explicitly enabled and supplied with reopen targets.
- Full FiberBundles are ephemeral; compact receipts are the durable result.
- AGI, million-node model-world and official external-baseline claims are excluded.

## Before merging this paper branch

The manuscript may be merged as a working preprint package. It must not be submitted to arXiv until the legal/IP, licence, frozen public release, exact-head validation and external review gates in `ARXIV_SUBMISSION_CHECKLIST.md` are complete.
