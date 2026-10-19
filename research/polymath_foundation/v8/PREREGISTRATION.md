# MOOL-POLYMATH-F8-001 — Semantic aliases, dimensional units and native ontology boundary

**Preregistered 2026-10-19 before F8 implementation, trial creation or score inspection.** Base commit `420268f853bd5603c8a2d25cca7958ad9657bfd8`, F7 PR #127. No edits to prior F1–F7 protocols/scores or epistemic-process freezes.

## Research claim and limitations

Can a provenance-scoped typed lexical ontology stored in the **real, persisted MoolBase GrapheneDB** normalize unfamiliar parameter/target labels and permitted unit changes, then select among three already authored procedures and safely abstain on ambiguous/unsupported cases? **This is a manually curated ontology and engineering transfer test, not autonomous semantic discovery, novel scientific theory or polymath intelligence.**

Independence: **No independently authored hidden test is available in this phase.** The generator is project-authored, public and versioned. Gold is separately held from the native process but not independently blind, because implementers know the generator and alias dictionary. Results MUST say `EXTERNALLY_BLIND_NOT_RUN`, never imply expert-independent holdout.

## Frozen task population

Use untouched v3 `generate(cards, seed=20261021)`: 72 in-library tasks, 60 valid arithmetic and 12 boundary-case abstentions. Replace **every** input and output name with one fixed, curated alternative (or a second name chosen by a deterministic hash of source task ID) so canonical names are not present in the provided structured query. Where supported, express corresponding values in permitted alternative units. Opaque IDs shuffled independently; no domain, mechanism ID, card ID or correct result in task packet.

Create 48 out-of-contract tests:
- 12 unsupported-unit cases (4 per domain) based on originally valid tasks;
- 12 unrecognised-domain questions with unknown variables and targets;
- 12 mixed-procedure cases (4 for each domain pair) where no single procedure may consume every field;
- 12 novel conflicting-alias cases (4 per domain) where two input labels map to the same canonical concept; reject rather than silently override.

Total: **120 cases, 60 valid calculations and 60 justified refusals** (including original 12 boundary cases). The project-authored alias vocabulary and rational unit policies are frozen before viewing scores. No post hoc tuning.

## Fixed ontology and conversion boundary

Two synonyms each for each existing input/output concept, with explicit `canonical, domain, field_kind, canonical_type, canonical_unit, alias_unit` and exact rational `mul_num/mul_den`. Every tuple must be unique and carry `source_status=unverified`, `author=project_authored`, `relation_kind=lexical_alias_not_scientific_equivalence`.

Unit policy: ordinary declared units and controlled deterministic transformations only (e.g. inventory daily demand in items/week converts by 1/7; lead days in hours by 1/24; normalized toy state `centistate` converts by 1/100). No general affine temperature/unit conversion, no arbitrary math parser and no scientific inference from lexical association. All integer output conversions back to v6 require exactly integral canonical values; nonintegral exact-rational values where an integer is required must abstain. Do not assume different packet sizes or uncertain measures equivalent. Conflicting aliases for one role, unknown aliases, unsupported target units, overflow and incorrect source provenance must reject rather than guess.

An alias does not become an empirically verified equivalence; it remains authored schema metadata. `GrapheneDB::metadata_search` and `get_node` must retrieve each actual alias entry from a reopened database before normalization; native aliases may not be replaced by hardcoded C++ answers or a Python lookup pretending to be native. `Observed/Verified` must never be inserted. Inputs are emitted into the unchanged F7 typed routing, then the untouched F6 native deterministic executor, and scored by the independent existing v3 toy oracle.

## Fixed comparison arms

**B0**: v7 exact-field schema baseline given the *unnormalized* query (no aliases or conversions), so may abstain. An intentionally non-semantic control, not a strong equivalently-informed benchmark.

**B1**: Python stdlib typed normalization using exactly the same curated alias-and-unit policy, followed by the unchanged F7 schema selector and v6 C++ procedure executor. Strong deterministic and fully informed non-MoolBase baseline.

**B2**: native MoolBase-based alias resolver (actual C++ stored/reopened alias nodes, provenance-audited) feeding the unchanged F7 native selector and v6 C++ executor. B1 and B2 should have equal decisions and numerical results; any disagreement is an engineering defect. Native persistence/auditability is the proposed incremental value, not a claim of superior problem-solving.

**B3 real LLM** and **B4 HypoKosh–DWM**: NOT_RUN unless actually integrated and executed with immutable raw receipts and budget accounting in a *separate* preregistered addendum. No simulated LLM, no fabricated model results. A later model comparison must give each arm the same alias table, tools, evidence, output restrictions and compute budget.

## Metrics and acceptance

- Exact baseline-normalized task, all 120 route decisions, executed numeric results and abstention reasons; per-category accuracy, hallucinated numeric rate, unsupported-unit rejection, source provenance, number of aliases used and native node IDs.
- Correlated/repeated source aliases do not create independent corroboration; authored alias and original procedure share a **single project-authored family**.
- Native B2 must match deterministic B1 exactly on all 120 selected/unselected routes, typed canonical input representations and final scores.
- Mutation controls: source family/status spoofing, duplicate aliases, alias collision, failed unit conversion, overflow, unknown procedure, composite query, missing/unsupported target and restart durability.
- Never score a refusal as a solved valid numerical question.
- Store source hashes, all raw task and route receipts, and negative outcomes in exact-head Actions artifact. Do not edit protocol, cases or scoring after result.

**Research gate not attempted:** independent third-party heldout application questions, learning unknown ontologies, blind analogy prediction, mechanism invention and falsification with external ground truth. Under no circumstances interpret a high F8 score as evidence of these capabilities.