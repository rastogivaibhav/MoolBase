# arXiv submission checklist

## Completed in this branch

- [x] One coherent manuscript centred on the implemented C++20 GrapheneDB system.
- [x] Narrow thesis: lineage-aware epistemic control and frontier-aware targeted evidence recovery.
- [x] Formal terminology for route, source, evidence-family, derivation, correlation groups, admissibility, and status projection.
- [x] Architecture and controller diagrams generated entirely from LaTeX source.
- [x] Paper-to-code artifact manifest.
- [x] Controlled benchmark methodology and exact aggregate results.
- [x] Negative finding and implementation correction documented.
- [x] Threats to validity and explicit non-claims.
- [x] Related work expanded beyond RAG/agents to provenance, temporal data, truth maintenance, argumentation, causality, and selective classification.
- [x] Reproducibility protocol.
- [x] Claim-to-evidence audit.
- [x] AI-tool-use disclosure.
- [x] Copy-ready arXiv metadata template.
- [x] Metadata/citation validation script.
- [x] Deterministic source-package script with SHA-256 manifest.
- [x] Paper build workflow and local Makefile.
- [x] Pre-submission gap review.

## Human/legal gates — cannot be delegated to software

- [ ] Confirm authorship and author order.
- [ ] Confirm accurate current affiliation, or omit affiliation rather than misrepresent it.
- [ ] Confirm author contact email and optional ORCID.
- [ ] Obtain employer/IP publication clearance.
- [ ] Confirm that no client, employer, confidential, personal, or restricted material is present.
- [ ] Select and approve the public code licence.
- [ ] Select the arXiv paper licence.
- [ ] Confirm third-party dependency and dataset redistribution rights.
- [ ] Confirm acknowledgements, collaboration, and funding disclosures.
- [ ] Review the arXiv source archive for hidden comments, secrets, internal URLs, and unnecessary files.

## Release gates before submission

- [ ] Make the repository public, or publish a public archival mirror.
- [ ] Replace the placeholder repository licence.
- [ ] Run `bash scripts/run_alpha_release_gate.sh` from a fresh clone of final `master`.
- [ ] Preserve the exact-head evidence bundle and checksums.
- [ ] Run ASAN and UBSAN gates; record platform exclusions.
- [ ] Complete dependency and licence review.
- [ ] Create an immutable release tag.
- [ ] Publish source/install artifacts, manifest, checksums, and release evidence.
- [ ] Archive the release and add its archival URL/DOI when available.
- [ ] Replace tag, commit, URL, and DOI placeholders in paper files and metadata.

## Manuscript quality gates

- [ ] `make -C paper check` passes in a clean TeX environment.
- [ ] Resolve LaTeX warnings affecting references, overfull tables, or layout.
- [ ] Confirm author name spelling and identity metadata.
- [ ] Check every numeric result against committed machine-readable output.
- [ ] Check every implementation claim against `CLAIM_EVIDENCE_MATRIX.md` and `ARTIFACT_MANIFEST.md`.
- [ ] Check reference metadata against primary paper or publisher records.
- [ ] Verify figures remain readable in grayscale and in the arXiv-generated PDF/HTML.
- [ ] Obtain at least one external technical review.
- [ ] Obtain one adversarial claim review from someone who did not build the system.
- [ ] Produce the final source archive with `make -C paper arxiv`.
- [ ] Run `python3 scripts/check_arxiv_package.py --paper-dir paper --strict`.

## arXiv account and submission gates

- [ ] Register/verify the submitting author account.
- [ ] Confirm whether endorsement is required for the selected category.
- [ ] Confirm the submitter is an author and all authors consent.
- [ ] Upload TeX source rather than a PDF generated from TeX.
- [ ] Confirm the processor and top-level file are detected correctly.
- [ ] Preview the generated PDF and inspect the compilation log.
- [ ] Re-enter metadata as ASCII; do not paste typographic Unicode punctuation.
- [ ] Keep the abstract at or below 1920 characters.
- [ ] Put final page/figure count and code URL in Comments.
- [ ] Leave Journal-ref and DOI blank unless a separate published version already exists.

## Recommended metadata

**Title**

GrapheneDB: Lineage-Aware Epistemic Control and Frontier-Aware Evidence Recovery for Agentic Systems

**Authors**

Vaibhav Rastogi

Affiliation and ORCID remain human-confirmation fields in `arxiv_metadata.json`.

**Primary category**

`cs.DB`

**Cross-list**

`cs.AI`

**Comments field template**

```text
<FINAL PAGE COUNT> pages, <FINAL FIGURE COUNT> figures. Technical preprint.
C++20 implementation and reproducibility artifact: <PUBLIC ARCHIVAL URL>
```

**Report number**

Leave empty unless an institution has assigned one.

**Journal reference / DOI**

Leave empty for an unpublished preprint. Add later only for a separately published version.

## Submission source package

Generate it rather than assembling it manually:

```bash
make -C paper arxiv
```

Expected source archive contents:

```text
main.tex
references.bib
main.bbl
MANIFEST.sha256
```

Do not include build directories, PDFs, logs, repository archives, benchmark datasets, Git history, credentials, internal notes, or unrelated source code.

## Suggested submission sequence

1. Complete legal/IP, identity, affiliation, and licence decisions.
2. Run the final exact-head release gate and freeze the code tag.
3. Publish/archive the release and update identifiers throughout the paper package.
4. Compile and externally review the exact manuscript commit.
5. Generate and inspect the clean arXiv source archive.
6. Confirm account/endorsement status.
7. Submit under `cs.DB`, cross-listed to `cs.AI`.
8. After announcement, add the arXiv identifier and DOI to the repository and release notes.

## Stop conditions

Do not submit while any of these remain true:

- the code licence is still a placeholder;
- employer/IP or author consent is uncertain;
- affiliation or identity metadata is unconfirmed;
- the referenced code release is mutable or inaccessible;
- reported numbers cannot be regenerated from the frozen commit;
- strict metadata/package validation fails;
- the abstract implies semantic truth, AGI, general causal discovery, universal reasoning improvement, or production readiness;
- the manuscript describes the implementation as Rust-first rather than the submitted C++20 codebase;
- the source archive contains hidden, confidential, unrelated, or unnecessary material.
