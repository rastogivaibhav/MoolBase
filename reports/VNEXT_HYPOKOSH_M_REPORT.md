# GrapheneDB vNext — Minimal HypoKosh-M Iteration

## Scope
This iteration intentionally avoids the full HypoKosh recursive/self-healing/research runtime. It extracts only the production-useful primitives into GrapheneDB server:

- TemporalFact-style node ingestion via `POST /v1/facts`.
- Temporal/provenance metadata persisted on nodes.
- Provenance-rich edge API via `POST /v1/edges/provenance`.
- Edge origin/role parsing: observed, discovered, inferred, reinforced, hypothetical; mechanistic, compressed, predictive, analogical, causal, contradicts, supports, supersedes.
- Evidence/derivation metadata: `source_id`, `span`, `observed_at`, `derived_from`, `promotion_status`, reinforcement hints.
- Path-bundle retrieval via `POST /v1/retrieve/bundle` and `/v1/retrieve/explain`.
- Query policy mode gating:
  - empirical: observed/discovered only
  - balanced: observed/discovered/inferred/reinforced, excludes hypothetical
  - exploratory/theoretical: all labelled paths
- Provenance validation via `POST /v1/admin/validate/provenance`.

## What was deliberately not added
- No recursive self-healing loop.
- No curiosity/analogy/discovery engine.
- No AGI/cognitive-substrate runtime.
- No LyapunovCritic dependency.

## Validation performed

### Build
- `graphenedb_server` built successfully.
- `graphenedb_tests` built and passed.
- `graphenedb_physical_lattice_primary_tests` built and passed.
- `graphenedb_dense_hex_lattice_index_tests` built and passed.

### Server smoke
A physical-lattice-primary server was started with API-key auth. The test inserted three temporal facts and provenance-rich edges:

- A deployment -> B leak: observed/mechanistic with evidence.
- B leak -> C outage: discovered/causal with evidence.
- A deployment -> C outage: inferred/compressed with `derived_from`.

Bundle retrieval from A returned both:

- compressed inferred shortcut A -> C
- mechanistic path A -> B -> C

This proves the vNext server can preserve mechanistic and compressed paths separately instead of collapsing them into one answer.

## Current limitations
- Bundle retrieval is server-side MVP using edge scans; it is correct for proof but should be moved into the core engine with indexes.
- Temporal fields are persisted as metadata, not yet strongly typed in the binary cell record.
- Evidence refs are currently metadata fields, not a first-class vector of EvidenceRef records.
- Hyperedges are still deferred.
- Retrieval quality benchmark versus vector-only is still the next proof gate.

## Recommendation
This is the right disciplined direction: GrapheneDB gains the strongest HypoKosh primitives without absorbing HypoKosh bloat. Next sprint should move bundle retrieval into the core engine and add a benchmark corpus for temporal supersession, contradiction preservation, and mechanistic-vs-compressed path recovery.
