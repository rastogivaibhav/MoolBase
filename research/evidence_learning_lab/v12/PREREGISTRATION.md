# MOOL-EVIDENCE-LEARNING-V12-001 — Evidence-to-Learning Lab, Phase 0

**Protocol freeze:** 2026-10-19, before v12 code and any v12 scores. Base is successful v11 experiment commit `0caeb861f91c94b9d0d734048ec16bb7bcc21e7c`.

## Two explicit milestones, not an intelligence claim

**P0 safety:** Change new C++ `GenericRelationIngestor` calls so unauthenticated caller-provided relations cannot silently become `EdgeOrigin::Observed`. Introduce a versioned, fail-closed trust choice in `GenericIngestOptions`: default `QuarantineUnverified` with origin `Hypothetical` and metadata `evidence_authority=unverified_input`; a deliberately named `LegacyObservedExplicit` opt-in preserves old data-generation contracts and annotates legacy provenance. Neither setting authenticates source bytes, ownership or scientific truth. Never transform already-persisted historical Observed edges automatically; issue a separate migration/audit requirement.

**Phase 0 lab:** Run at least **three controlled, deterministic inventory-fault cases** (receipt delay, stale snapshot, misconfigured order rule) with 3 candidate hypotheses each. Generate a strictly separate ground-truth intervention record, never feed truth to the native reasoner. Each case has time-ordered initial signal and a subsequently disclosed contradictory/specific synthetic observation. Use real GrapheneDB, native source-aware `GenericRelationIngestor` (default quarantine), actual `CompleteHypoKoshRuntime` under G0/G1/G2 ablations with identical GrapheneDB data/options and no ModelWorld truth promotion. The native observations are internally injected, never externally authenticated. For hypothesis reasoning in Theoretical mode, use explicitly Hypothetical graph evidence; no mode should call it verified.

Track for each incident and evidence stage: injected cause (evaluation only), candidate hypotheses, observed event payload/source classification, native graph edge counts, selected hypothesis if any, governed status, DWM challenge/reopen, safe refusal and time elapsed. Do not claim learning from persisted audit history unless future unseen-case predictions actually improve. The comparison with real LLM-only and LLM+retrieval arms is intentionally **NOT_RUN in Phase 0**; prevent representing G0 evidence projection as the equivalent of an LLM baseline. No claims of strong science or generalization on three authored cases.

## Acceptance / failure

1. **Legacy source trust gate:** new default = unverified/quarantined, not Observed; explicit compatibility mode preserves Observed and carries `legacy_unverified` metadata; both persist across DB reopen. Old packaged client compatibility is not automatic and must be documented.
2. **New native source gating tests:** forged source label, missing source and invented SHA cannot grant Observed. Explicit legacy path remains visible, versioned and warning-bearing; regressions run alongside existing source-integrity tests.
3. **Real native incident workflow:** at least 3 faults x 2 evidence times x G0/G1/G2 = 18 actual `CompleteHypoKoshRuntime` calls; same evidence per arm, raw unverified signals, time-ordered receipts, separate held scoring labels. Full outcome metric under strict `Observed` authority is **NOT AVAILABLE** because no external authenticator is connected.
4. **Evaluation:** report answer coverage, matched synthetic intervention label when answer emitted, unsafe factual commitments, inability to revise, and any null results; report exact provenance. Never score no-response as an accurate diagnosis, and never count author's synthetic labels as independent adjudication.
5. All files, exact source SHA, logs and negative results in GitHub CI artifact with checksums. No F1–F11 original files or freeze protocols edited.

## Blockers before production or polymath claims

GenericRelationIngestor is not the only direct `GrapheneDB::put_edge` entry point: broader ingress audit, authorisation and existing-Observed migration remain unresolved. Separately authenticated external enterprise sources, independent empirical outcome adjudication, real LLM baselines with matched data/compute, and post-feedback unseen-case uplift are subsequent milestones. This phase establishes a demonstrator and a bounded hardening step, **not** completed production provenance protection or confirmed autonomous learning.
