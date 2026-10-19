# MOOL-POLYMATH-F11-001 — Source identity is not source authority

**Protocol-first preregistration**, 19 October 2026, prior to implementing F11 tests or scoring. Based on F10B head `6d7c7f4bba893afca80f5b9606d1a9266587945a`. Do not rewrite any F1–F10B data or scoring.

## Research premise
A caller-supplied `source_id` is merely a metadata claim, not proof of content integrity, independent origin or scientific truth. Existing `GenericRelationIngestor` stamps generic input `EdgeOrigin::Observed` without source authentication. The research question is whether an **independent strict admission boundary** can reject spoofed evidence before graph ingestion, while leaving unverified synthetic material explicitly unverified. F11 must not claim this fixes every production ingestion path.

## Design and expected controls
Implement an **isolated fail-closed Python stdlib reference adapter**, not a replacement for the production C++ ingestion API. It receives (a) a fixed curator-provided source registry, (b) exact raw source bytes, and (c) asserted typed claims with UTF-8 byte offsets. The registry is loaded from a separately supplied trusted runtime file, not fabricated by an LLM candidate; registry identity is itself NOT an authenticated external authority absent protected distribution.

- SHA-256 content hash mismatch, missing source, missing exact source span, invalid UTF-8 boundary, noncanonical source reference, expired/revoked source and forged family IDs MUST be refused.
- Do not trust a caller-supplied `Observed`/`Verified` label; reject any positive verified-truth claim.
- A valid registry+content+span yields only `SOURCE_CONTENT_INTEGRITY_CHECKED_UNVERIFIED`, never `Verified`. Duplicate derived copies from one source must share one declared canonical family in this *synthetic* fixture, with no independent-family score improvement.
- Every decision receives a receipt recording asserted origin, stated provenance, admit/refusal reason, digest and separately declared adjudication status. Absolutely no external scientific adjudication is performed.
- Include at least 12 different negative controls and clean, correlated, revoked and tampered positives. Exact test names and expected statuses fixed in the committed test code before CI.

An additional **native C++ characterization test** uses the unchanged existing `GenericRelationIngestor` to demonstrate and receipt whether a generic, source-less input still receives `EdgeOrigin::Observed`. This is a documented known boundary, NOT a claimed exploit or a green security fix.

## Pass/fail and scientific claim limits
- Engineering acceptance: 15+ stdlib negative/positive tests pass; actual native characterization compiles/runs; all receipts and checksums archived; no F1–F10B files changed; no false claim of external verification.
- Truth admission test: **none** of the project-authored fixture data becomes an externally verified fact. If any does, fail.
- No new independently authenticated source, true causal intervention, evidence-family proof, model training or HypoKosh/DWM superiority will be claimed.
- Product hardening remains blocked: actual production entry points must be refactored to use a trusted source-admission contract with authenticated callers and third-party provenance, not just opt-in experimental Python code.

Follow-up F12 would independently validate real signed source origins and temporal outcomes, require secured registry distribution and compare the same external evidence in G0/G1/G2.
