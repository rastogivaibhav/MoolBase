# MoolBase Polymath Foundation v1 — Knowledge, Ontology and Research Agenda Contract

**Status:** Research prototype / controlled fixture, 18 October 2026.
**Scientific claim:** NO polymath capability established; no autonomous science; no externally authenticated knowledge. Implements a minimal reviewable vertical slice of metadata validation, directional relation safety and transparent research-question triage, without changing frozen EP-PROCESS V1/V2/V3 results.
**Base:** branch directly from \`master\`, independent of unmerged P0–P4 experiments.

## Why this work precedes more predictive models

The project's earlier P1–P4 research addressed outcome updating and contextual learning, not independent knowledge development, abstraction and question selection. Before the system explores literature or generates hypotheses on its own, it must separate raw evidence, domain concept meaning, cross-domain analogy and a proposed experiment. An ungrounded lexical association or a reversed causal edge must never become a high-confidence scientific conclusion.

This release is deliberately small and **read-only at the research layer**. It introduces neither a background scheduler nor a new LLM. The three-domain fixture is synthetic and hand-curated, and all discovered "research questions" are triaged from provided candidate inputs; they are *not model-generated discoveries*.

## Existing durable engine and key corrections

MoolBase's extraction API has stable source-scoped external IDs, edge evidence fields, metadata, idempotency and transaction validation; the core \`EvidenceRef\` stores source lineage and temporal validity. \`RelationOntology\` supports aliases and domain/range types. This foundation adds:
1. \`RelationOntology::resolve(predicate)\` -> \`{canonical_id, swap_endpoints}\`, where inverse phrases are direction-bearing rather than ordinary synonyms.
2. \`RelationOntology::canonicalise(predicate)\` retains legacy optional-string ABI but intentionally yields no result for inverse aliases to prevent misleading forward-only use; new ingestion calls \`resolve\`.
3. Generic relation ingestion swaps stored subject/object **and domain/range type validation** for inverse phrases, preserving the original predicate and direction metadata.
4. \`caused_by\` and \`supported_by\` reverse; \`calls\`, \`uses\`, \`requires\` and \`depends_on\` are distinct, instead of being silently merged.
5. Existing \`caused\`, \`assigned_vehicle\` and \`current_depot\` relations still resolve under the retained prototype conventions.

**API compatibility boundary:** callers that previously expected \`canonicalise("caused_by") == "caused"\` must adopt \`resolve()\`. Some former conflated aliases no longer map to their prior predicate; this is an intentional semantic safety change. This is NOT a full formally complete OWL/RDF ontology implementation. The extension should be reviewed for backward-compatibility and consumer migration before being merged.

## Proposed metadata schema

\`metadata.schema.json\` specifies a portable *illustrative fixture* using:
- **Source registry:** stable id, source-family id, declared kind, URI, licence, integrity, methodology; v1 rejects fabricated external authentication and only accepts \`fixture://\` records.
- **Domain registry:** domain identity, ontology version and competence-test references.
- **Concept:** domain-qualified id, definition, prerequisites, mechanism roles, assumptions, boundaries and source IDs.
- **Cross-domain mapping:** source/target concepts, explicit structural roles, counterexample boundary, question, prediction, falsifier and heuristic research-priority inputs.
- **Knowledge gap:** concept, question, plausible research method and estimated information-gain/feasibility/cost/risk values.
- **Competence tests:** task identities with answer access withheld; no competence outcome is claimed because the fixture contains *no assessed answers*.

Use stable typed identities rather than assuming words are concepts. Every concept maps back to declared source records. A source family is a *claimed lineage ID*, not proof that the sources are independent.

**Scientific production extensions (not implemented yet):** External source URIs, DOIs, real content hashes, authentication receipts, event/validity time, unit systems, versioned formal ontology mapping, distinction between observed vs derived assertions, full source licensing and permissions, calibration of quality estimates, ontology-equivalence review and genuine competency evaluations. Before enabling real-world ingestion, extend the schema under a new version and attach authenticated source quality assessments. Do not pass real research papers through this synthetic fixture validator by stripping provenance; such conversion would be epistemically unsafe.

Standards to review for compatibility: PROV-O (provenance), SKOS (mapping/equivalence), SHACL (constraints), DCAT (datasets), JSON-LD (linked identity). These are *design inputs*, not technologies implemented here.

## Association boundaries

A candidate **structural** mapping must have:
- concepts from different baseline domains;
- at least **two relationship/role labels actually present on both concepts**, rather than words merely appearing in their titles;
- explicit scope/boundaries and at least one potential counterexample;
- a concrete predeclared prediction and falsifier;
- at least two distinct *claimed* evidence source families to be eligible for the research queue.

A mapping with only lexical similarity is rejected as structural evidence; a mapping backed by one family is deferred. Even an admitted candidate stays \`candidate_unverified\` and is not promoted to empirical truth.

**Important limitation:** Hand-labelled shared roles are easy to game. The prototype checks consistency of a declared mapping, not actual mechanism equivalence or verified causal transfer. Real evaluation must use hidden-target structurally remapped tasks and independent evaluators.

## Research selection contract

The bounded, deterministic triage prototype evaluates only provided mapping and gap records. Its fixed illustrative score is:

\`priority = clip(0.35*information_gain + 0.25*transfer_value + 0.25*feasibility - 0.15*cost - 0.15*risk, 0, 1)\`

This is **not** a calibrated value-of-information model. It is a transparent ordering baseline against which a learned research policy can later be compared. Rank order is stable; every record includes explanation, competence precondition, source independence status and \`external_experiment_permission=false\`.

Neither this script nor the C++ ontology resolver selects an external website, fetches literature, executes an experiment, trains a neural model or writes \`Verified\` facts to MoolBase.

## Reproduction

\`\`\`bash
python3 research/polymath_foundation/v1/test_foundation.py
python3 research/polymath_foundation/v1/foundation.py \
  --input research/polymath_foundation/v1/three_domain_fixture.json \
  --output reports/polymath_foundation_v1/agenda.json

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON
cmake --build build --parallel 2 --target graphenedb_polymath_relation_direction_tests
ctest --test-dir build -R graphenedb_polymath_relation_direction_tests --output-on-failure
\`\`\`

The CI workflow \`.github/workflows/polymath-foundation-v1.yml\` runs C++ direction tests and Python adversarial metadata/triage tests; its receipt contains the versioned fixture hash and deterministically ranked agenda. Report null/failed gates unchanged.

## What should happen next (separately preregistered)

**R1 / domain foundations:** Provide authentic, licensed texts and source families. Hold back test answers and measure actual within-domain comprehension, prerequisite transfer, retrieval and calibration. If knowledge acquisition does not improve held-out subject performance, do not run association experiments.

**R2 / cross-domain analogy:** Compare typed structural matching with dense retrieval, lexical matching, graph matching and a strong same-budget language model. Include reversed causal directions, near-synonyms, correlated copies and mismatched scientific units as negative controls. Success requires hidden-target predictive advantage, not just candidate-count growth.

**R3 / research policy:** Compare the heuristic queue, random choice and a bounded LLM/learned selection policy using identical access to facts, source quality and compute. Verify research question utility with a private external oracle and falsifiable predictions.

**R4 / offline incubation:** Freeze future problem set *after* source ingest, before the offline interval; precommit all hypotheses without target-task disclosure. Match total inference budget including the offline interval; independently assess later transfer and costs.

Never describe this Phase v1 as evidence that MoolBase has become a polymath, has invented a theory, or has learned an autonomous research policy.
