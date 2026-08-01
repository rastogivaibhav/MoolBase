# GrapheneDB arXiv pre-submission review

## Review verdict

The paper package is technically coherent and substantially stronger than the earlier whitepapers because it describes the merged C++20 implementation, reports a controlled negative result, and keeps the main claim inside the available evidence.

It is suitable for merge as a **working preprint package**. It is **not yet suitable for arXiv submission** until the critical gates below are closed.

## What this review verified

- The manuscript is centred on one falsifiable systems contribution: lineage-aware epistemic control plus frontier-aware targeted evidence recovery.
- The 700-execution intervention result is reported as controlled mechanism evidence, not semantic accuracy.
- The paper distinguishes raw route count from configured independent evidence-family count.
- The old unchanged-completed-bundle stopping defect is documented as a negative finding that changed the implementation.
- Causal edges are described as provenance-bearing reasoning assertions, not proof of causal identification.
- The Lyapunov-inspired critic is bounded as a finite-trajectory diagnostic, not a global convergence proof.
- Cross-dataset work is labelled structural rather than answer-quality evaluation.
- The earlier Rust-first, AGI, million-node, recursive self-improvement, and unofficial-baseline claims are not presented as current results.
- Paper source, bibliography, metadata template, validation, packaging, claim audit, and reproducibility instructions are included.

## Critical blockers before submission

### Legal and identity

- [ ] Employer/IP publication clearance is documented.
- [ ] Author affiliation is confirmed and accurately represented.
- [ ] Author email and optional ORCID are confirmed.
- [ ] Code licence replaces the current proprietary/evaluation placeholder.
- [ ] Paper licence is selected in the arXiv submission flow.
- [ ] Dependency, dataset, and third-party notice review is complete.
- [ ] No client, employer, private key, internal URL, hidden comment, or confidential artifact exists in the source archive.

### Frozen public artifact

- [ ] Repository or archival mirror is publicly accessible.
- [ ] Immutable code tag and exact commit are created after the final release gate.
- [ ] Paper cites the immutable tag and public archival URL.
- [ ] Release source, evidence bundle, manifest, and checksums are published.
- [ ] A software archive DOI is added when available; a moving branch URL is not treated as archival evidence.

### Validation

- [ ] `scripts/run_alpha_release_gate.sh` passes from a clean clone of the final exact commit.
- [ ] Paper CI or an equivalent clean TeX Live build passes on the exact paper commit.
- [ ] `python3 scripts/check_arxiv_package.py --paper-dir paper --strict` passes after placeholders are replaced.
- [ ] `make -C paper arxiv` creates a clean source archive and checksum.
- [ ] Every number is checked against committed machine-readable output.
- [ ] At least one external technical reviewer and one adversarial claim reviewer sign off.

### arXiv account and metadata

- [ ] The submitting arXiv account is registered and the submitter is an author.
- [ ] Endorsement status for the chosen primary category is checked before the submission window.
- [ ] The final primary category is reviewed against the paper emphasis; moderators may reclassify it.
- [ ] Metadata is ASCII and the abstract remains at or below 1920 characters.
- [ ] Comments contain the final page and figure count plus the public code/artifact URL.
- [ ] Journal reference and DOI fields remain empty unless a separate published version exists.

## Research gaps that remain after submission

These are limitations, not administrative blockers, provided the paper continues to state them clearly:

- No end-to-end semantic answer-quality claim.
- No official head-to-head comparison with graph databases, vector databases, RAG systems, or agent frameworks.
- No statistical generalisation from the topology-preserving deterministic variants.
- No complete public 2,500-record benchmark result in the present claim set.
- No million-node rich model-world proof.
- No global convergence proof.
- No production security certification, distributed deployment proof, or long-duration soak.
- No validated implementation-outcome feedback loop or Kosh-aware model training result.

## Category review

`cs.DB` is defensible when the paper is presented as an embedded evidence database, provenance/lineage representation, persistence layer, and query-time control mechanism. `cs.AI` is a defensible cross-list because the main use case is governed agent reasoning and recursive evidence recovery.

If the final manuscript shifts toward agent behaviour and removes most database implementation detail, reverse the order. Do not submit the same work separately to both categories.

## Final claim boundary

The strongest currently supportable statement is:

> GrapheneDB implements a lineage-aware evidence-control runtime that prevents configured correlated paths from inflating independent support and uses frontier progress to govern bounded targeted recovery in controlled graph scenarios.

The paper must not be marketed as a truth engine, AGI substrate, universally self-improving reasoner, causal discovery system, enterprise-ready database, or proven replacement for mature graph/vector stores.
