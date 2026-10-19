# MOOL-EVIDENCE-LAB-V13-001 — Time-ordered operational event boundary

**Frozen 19 October 2026, before writing v13 implementation or viewing any v13 test outcomes.** Parent exact validated v12 commit `423e484e4b2e433a97c7a042dfcec5c621b49aa9`, 47 successful/9 skipped checks. Older research receipts and scores immutable.

## Question
Can a deterministic ERP/WMS simulator create a reproducible, source-attributed, time-ordered event stream, and can an evaluator withhold the injected cause without leaking it through the evidence payload or parser? This is a **research data-infrastructure gate**, not yet an actual externally validated end-to-end AI reasoning comparison.

## Design
Create a project-authored event feed of at least 3 faults: receipt-posting delay, WMS-snapshot staleness and replenishment-rule misconfiguration. Each incident should have an ambiguous T0 observation and later T1 discriminating observations, at least 2 independently modelled producer names (`erp`, `wms`, `fault-controller`). The fault-controller is the synthetic experimenter and never serves evidence to the reasoning arms. All observations have event ID, simulated producer ID, UTC event timestamp, recorded timestamp, event type, inventory SKU, JSON payload and per-event SHA-256 computed over an exact canonical JSON representation. An outer receipt binds complete feed bytes.

The generator has two separate outputs: `observations.jsonl` (available to reasoning systems) and `evaluator_only_truth.json` (withheld). The public repo contains generation code, so this is **not independently blind**. Synthetic producer labels are **not external authentication**. No `EdgeOrigin::Observed` claims are made from such labels.

Predeclared negative controls: duplicate IDs, non-monotonic timestamps per source, evidence carrying `injected_cause`/`ground_truth`/other answer keys, evidence altered after hashing, invalid UTF-8, producer outside allowlist, missing required fields, invalid digest, and attempted externally Verified/Observed labels. The evaluator must verify exact recomputed hashes, schema and omission of protected truth fields. A reasoner cannot read truth solely from the provided observations file; the repository itself remains public.

## Acceptance
- Deterministic generation twice gives byte-identical files under a pinned seed.
- >=3 synthetic incidents, two stages each, all protective controls pass and manifest SHA-256 reproducibility checked.
- Every later event timestamp > initial event timestamp for its case; evidence from multiple producers; no cause keys or explicit case diagnosis leaked.
- Explicitly report `actual_native_MoolBase_reasoning=NOT_RUN`, `LLM_baselines=NOT_RUN`, `independent_adjudication=NO`, `external_authentication=NO`, `causal_learning=NOT_DEMONSTRATED` for this gate.
- Separate next experiment should connect the validated event feed through a *trusted* ingestion adapter to native MoolBase, then execute matched LLM/RAG/native comparison. Do not portray this deterministic fixture as commercial readiness.

Do not change production C++ ingest, DWM threshold, v12 scoring or existing frozen results in v13.
