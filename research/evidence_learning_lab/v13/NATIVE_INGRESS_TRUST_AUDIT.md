# P0 Native Source Authority Audit — Open release blocker

**Date:** 19 October 2026. Source reviewed in MoolBase research v12/v13 ancestry. This is a review record, not a blanket security fix.

## Verified API surfaces

1. `include/graphene/types.hpp::EdgeInput` still defaults `origin=EdgeOrigin::Observed`, and `GrapheneDB::put_edge()` allows a direct caller to supply that origin. The v12 `GenericRelationIngestor` default quarantine **does not** intercept direct graph insertions.
2. `include/graphene/types.hpp::ExtractionRelation` defaults to `EdgeOrigin::Observed`. `GrapheneDB::put_extraction()` is another entry point requiring independent authentication-context and evidence-lifecycle review.
3. `GrapheneDB::put_batch()` writes caller-provided `EdgeInput` records, and its provenance handling requires inspection.
4. `GenericSourceTrustMode::LegacyObservedExplicit` is public API and does not itself authenticate its caller. This is a temporary backwards-compatibility escape hatch, **not safe to expose to untrusted agents or network requests**.
5. Existing persisted edges marked Observed are not migrated; do not assume new source policy retroactively authenticates historical source records.

## Proposed production-safe boundary

Introduce an application-controlled `IngestAuthorityContext` issued by a trusted source connector, not inferred from user-supplied `source_id`, hash, LLM JSON, evidence text, file path or self-signed label. At public/network/tool ingress, default all unreviewed claims to `Hypothetical` with `authority=unverified`. Guard native Observed creation through auditable named methods with explicit trusted issuer configuration, source-document byte checks, exact assertion offsets and recorded signatures; even this only authenticates origin, **not empirical truth**. Require explicit reviewer-approved migrations for existing Observed and explicit legacy tests.

## Mandatory negative tests / acceptance

- Forged source ID, arbitrary SHA, duplicated derived families, expired/revoked/rotated source credential, unauthorized LegacyObservedExplicit and direct `put_edge`/batch/extraction cannot produce authenticated Observed.
- Any attempted provenance promotion has a durable denial/audit receipt; historical Observed rows remain accessible for compatibility but flagged `legacy_unverified` until reviewed.
- Full end-to-end C++, C/API/server integration tests across old file versions, Linux/Windows/macOS, storage restart and source revocation.
- Only after this review plus independent external attestation and empirical fault outcomes may real customer ingestion be considered for production.

## V13 research boundary

The 6-case ERP/WMS lab runs a SQLite *stand-in*, not actual ERPNext, and all actual native evidence edges are explicitly `Hypothetical`. It uses Theoretical mode and does not claim the above P0 is resolved. The source-generated outcome controller and public event adapter are project-authored and do not confer independent validity.
