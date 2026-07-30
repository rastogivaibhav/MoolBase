# GA Progress: Extraction Contract

Date: 2026-07-04

## What changed

- `ExtractionInput` now carries an explicit `schema_version`.
- `kExtractionSchemaVersion` is the stable v0.5 extraction schema version.
- `put_extraction()` rejects unsupported extraction schema versions with `UnsupportedMode`.
- `ExtractionInput` now supports optional source-level provenance:
  - `source_uri`
  - `extraction_run_id`
- `ExtractionRelation` now supports first-class evidence fields:
  - `evidence_id`
  - `evidence_uri`
  - `evidence_text`
- Ingested nodes and edges persist extraction schema, source URI, and extraction run ID metadata.
- Ingested edges persist relation evidence metadata while still preserving caller-provided relation metadata.

## Contract decision

`put_extraction()` schema version 1 is the stable v0.5 ingestion boundary.

The extractor remains outside the database core. It owns chunking, embeddings, relation discovery, evidence selection, confidence scoring, and optional lattice hints. GrapheneDB owns validation, source-scoped identity, duplicate suppression, lattice validation/placement, and durable commit.

## Local verification

Focused build passed:

```text
cmake --build build-release --target graphenedb_extraction_ingest_tests graphenedb_cli_extract_tests graphenedb_rc5_fault_injection_tests graphenedb_tests --config Release
```

Focused CTest passed:

```text
ctest --test-dir build-release -R "graphenedb_extraction_ingest_tests|graphenedb_cli_extract_tests|graphenedb_rc5_fault_injection_tests|^graphenedb_tests$" --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 4
```

## Remaining evidence

The extraction contract should still be exercised in the full GA readiness harness and package consumer verification on approved release hosts.
