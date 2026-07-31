# arXiv submission checklist

## Completed in this branch

- [x] One coherent manuscript centred on the implemented C++20 GrapheneDB system.
- [x] Narrow research thesis: lineage-aware epistemic control and frontier-aware targeted evidence recovery.
- [x] Controlled benchmark methodology and exact reported results.
- [x] Negative finding and implementation correction documented.
- [x] Threats to validity and explicit non-claims.
- [x] Related-work section for RAG, GraphRAG, Self-RAG, ReAct and Reflexion.
- [x] Reproducibility protocol.
- [x] Claim-to-evidence audit.
- [x] AI-tool-use disclosure.
- [x] Bibliography source.

## Human/legal gates — cannot be delegated to software

- [ ] Confirm authorship and author order.
- [ ] Confirm preferred affiliation or “Independent Researcher”.
- [ ] Obtain employer/IP publication clearance.
- [ ] Confirm that no client, employer or confidential data/code is present.
- [ ] Select and approve the public code licence.
- [ ] Select the arXiv paper licence.
- [ ] Confirm third-party dependency and dataset redistribution rights.
- [ ] Confirm whether acknowledgements must name any employer, collaborator or funding source.

## Release gates before submission

- [ ] Make the repository public, or publish a public archival mirror.
- [ ] Replace the placeholder repository licence.
- [ ] Run `bash scripts/run_alpha_release_gate.sh` from a fresh clone of final `master`.
- [ ] Preserve the exact-head evidence bundle and checksums.
- [ ] Run ASAN and UBSAN gates; record any platform exclusions.
- [ ] Complete dependency and licence review.
- [ ] Create an immutable release tag.
- [ ] Publish source/install artifacts, manifest and checksums.
- [ ] Replace tag/commit placeholders in `paper/REPRODUCIBILITY.md`.
- [ ] Add the public repository/release URL to the paper.

## Manuscript quality gates

- [ ] Compile with a clean LaTeX installation.
- [ ] Resolve all LaTeX warnings affecting references, tables or layout.
- [ ] Confirm author name spelling and contact email.
- [ ] Check every numeric result against committed machine-readable output.
- [ ] Check every implementation claim against `paper/CLAIM_EVIDENCE_MATRIX.md`.
- [ ] Check references and author lists against primary paper records.
- [ ] Add architecture figure only if it can be generated reproducibly and remains readable in grayscale.
- [ ] Obtain at least one external technical review.
- [ ] Obtain one adversarial claim review from a reader who did not build the system.

## Recommended arXiv metadata

**Title**

GrapheneDB: Lineage-Aware Epistemic Control and Frontier-Aware Evidence Recovery for Agentic Systems

**Primary category**

`cs.DB`

**Cross-list**

`cs.AI`

**Keywords**

Evidence lineage; agent memory; graph reasoning; epistemic control; retrieval-augmented generation; contradiction; abstention; recursive retrieval; provenance.

**Comments field draft**

Technical preprint. Includes an open-source C++20 implementation and reproducibility package for controlled intervention and structural evidence benchmarks. Experimental research system; not a semantic truth engine or production decision authority.

## Submission source package

Include:

```text
main.tex
references.bib
figures/          # only source-required figures
```

Do not include:

```text
build/
*.aux
*.log
*.bbl             # include only if required by chosen arXiv build strategy
*.blg
full repository archives
benchmark datasets not required to compile the paper
```

## Suggested submission sequence

1. Complete legal/IP and licence decisions.
2. Run the final release gate and freeze the code tag.
3. Update reproducibility identifiers and repository link.
4. Compile and externally review the manuscript.
5. Create the public GitHub release.
6. Submit to arXiv under `cs.DB`, cross-listed to `cs.AI`.
7. After arXiv publication, add the arXiv identifier to the repository README and release notes.

## Stop conditions

Do not submit while any of these remain true:

- the code licence is still a placeholder;
- the referenced release is mutable or private without an archival public copy;
- reported numbers cannot be regenerated from the frozen commit;
- employer/IP rights are uncertain;
- the abstract implies semantic truth, AGI, general causal discovery or production readiness;
- the manuscript describes the implementation as Rust-first rather than the submitted C++20 GrapheneDB codebase.
