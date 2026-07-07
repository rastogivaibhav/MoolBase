# Extraction Ingestion

GrapheneDB now has a first-class extraction ingestion API:

```cpp
ExtractionInput input;
input.schema_version = graphene::kExtractionSchemaVersion;
input.source_id = "research-pack-001";
input.source_uri = "file:///research-pack-001.md";
input.extraction_run_id = "extract-20260704-001";
input.nodes.push_back(...);
input.relations.push_back(...);

ExtractionResult result;
db.put_extraction(input, &result);
```

This is the intended boundary between an extractor service and the storage engine. The extractor is responsible for turning source material into stable external IDs, text content, vectors, semantic roles, relations, and optional lattice annotations. The database is responsible for validating, placing, indexing, and durably committing that structure.

## Stable Identity

Every extracted node must include `external_id`, scoped by `ExtractionInput::source_id`.

GrapheneDB stores these metadata keys on each committed node:

- `graphene_source_id`
- `graphene_external_id`
- `graphene_scoped_external_id`
- `graphene_ingest=extraction-v1`
- `graphene_extraction_schema=1`
- `graphene_source_uri`, when supplied
- `graphene_extraction_run_id`, when supplied

`put_extraction()` is idempotent by default. Re-importing the same `source_id` and `external_id` reuses existing nodes instead of creating duplicates. Relations also receive a deterministic `graphene_relation_key`, so duplicate relation rows inside one batch and repeated imports do not duplicate existing extraction edges.

## Relation Evidence

`ExtractionRelation` has first-class evidence fields:

- `confidence`
- `evidence_id`
- `evidence_uri`
- `evidence_text`

These are stored as edge confidence plus `graphene_evidence_*` metadata. Custom relation metadata is still preserved. This makes `put_extraction()` the stable v0.5 durable ingestion boundary: the extractor owns evidence discovery, and GrapheneDB owns validation, identity, idempotency, placement, and commit.

## Lattice Placement

If an extractor supplies `ExtractionNode::lattice`, GrapheneDB validates it. If lattice placement is missing and `place_missing_lattice` is true, GrapheneDB assigns the next open axial hex coordinate on the requested layer along a neighbor-preserving row. That default favors valid extracted chains over visual spiral packing.

This gives two implementation modes:

- extractor-authored lattice coordinates for domain-specific placement
- database-authored deterministic placement for simple ingestion

When `DBOptions::require_lattice` is enabled, every inserted extraction node must end up with a coordinate. Lattice bonds still obey the same neighbor validation rules as direct `put_edge()` and `put_batch()` calls.

## CLI

```bash
graphenedb_cli extract-tsv <path> <dim> <source-id> <tsv-file>
```

TSV format:

```text
external_id<TAB>content<TAB>comma_vector<TAB>signature<TAB>incident<TAB>role
```

`role` may be `root`, `symptom`, `impact`, or `node`. Adjacent rows are imported as `Supports` relations with `Sigma` lattice bonds.

## What The Extractor Still Needs To Do

The built-in path is deterministic and durable, not an LLM or material-science parser. A production extraction service should add:

- chunking and provenance tracking for source documents
- embedding generation and dimension validation before commit
- stable external ID generation from source URI, section, and semantic span
- relation extraction with confidence, evidence IDs, evidence URIs, and evidence text
- optional defect, bond, and layer assignment
- retry-safe batching around `put_extraction()`

The analogy stops at graph/lattice computation. GrapheneDB does not simulate carbon physics, electron behavior, or material properties.
