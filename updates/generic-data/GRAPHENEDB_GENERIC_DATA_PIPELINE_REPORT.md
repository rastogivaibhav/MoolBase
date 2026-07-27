# GrapheneDB Generic Data and Tokenised Dataset Validation

## Scope

The canonical relation pipeline was extended beyond person/family/media relations. Graphene now ingests arbitrary directed facts from structured and tokenised datasets, while preserving source atoms and running the complete Graphene → HypoKosh → dialectic → governed projection sequence.

## Added domain-neutral inputs

- JSON/JSONL edges using subject/predicate/object or from/edge/to keys
- TSV and pipe-delimited edge records
- RDF/N-Triples-inspired records
- Token-tagged records using SUBJ/REL/OBJ or S/P/O markers
- Decimal-bearing and punctuation-bearing token values preserved as data
- JSON path-query contract with arbitrary relation names

Example query:

```json
{"start":"shipment-22","relations":["contains","assigned_vehicle","current_depot"],"terminal_type":"depot"}
```

## New frozen domain tests

Eight independent cases covered supply chain, software dependency, IoT telemetry, finance lineage, scientific data, token-level linguistic data, healthcare records and manufacturing provenance.

Results:

- Exact answer: 8/8
- Full Graphene/HypoKosh/dialectic pipeline attested: 8/8
- Canonical relation pipeline attested: 8/8
- Mean CLI latency: 4.346 ms
- All answers retained a source-grounded reasoning graph

## Regression

Release build completed successfully. The complete CTest suite passed: 45/45 tests, 0 failures, 28.42 seconds.

## Architectural meaning

The system is no longer constrained to a fixed ontology of human relationships. Relations are normalised as arbitrary predicates and traversed through a generic path contract. Domain ontologies can be layered above this substrate, but the storage and reasoning path is now data-model agnostic.

## Remaining limitations

- Natural-language relation discovery remains pattern-based for unstructured prose.
- Generic structured datasets require an explicit path query rather than automatic intent-to-path induction.
- Entity resolution, schema alignment, cardinality constraints and datatype validation remain future work.
- Tokenised datasets are supported as edge records; full BIO/IOB2 sequence-label import is not yet implemented.
