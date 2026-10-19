# MOOL-EVIDENCE-LEARNING-V14-001 — Cause-selection leakage ablation

Preregistered **19 October 2026** before any V14 benchmark implementation or outcome inspection. Parent SHA `8ad8eefde5c4787fed05e4f8f580f5414aa2447e` (v13 executed SQL events/native/7B comparator). This is **not** independent blind science.

## Hypothesis and design
V13's 6/6 G1 and G2 results may come from **the hand-authored v13 `decision()` classifier**, which emits already-labelled `Supports` and `Contradicts` edges for H-A, H-B, H-C. Therefore native reasoning may be consuming the classifier's answer rather than discovering a cause from raw ERP/WMS events.

Freeze the six original **identical** SQL event streams, its T0/T1 cutoffs and hidden controller labels. Do not touch V13 scores. Run new native G1/G2 against a **neutral observation-only adapter**: extract the T0/T1 source IDs, raw integer key/value state fields, canonical event digest and timestamp; do **not** run `decision()` or emit relation lines that reference `H-A`, `H-B`, `H-C`, `supports`, `contradicts`, or inferred root causes. Restrict source mode to **QuarantineUnverified** and QueryMode::Theoretical; preserve the actual C++ `GrapheneDB` and `CompleteHypoKoshRuntime` executions. Candidate H-A/B/C nodes may be registered as hypothesis vocabulary, but no adapter-generated hypothesis edges are allowed.

Expected 6 incidents × 2 time cutoffs × 2 native arms = **24 real native calls**, plus a scorer that compares V14 with the frozen V13 R0/G1/G2 results on the **same content-hashed ERP/WMS input streams**. Confirm that any new proposal derives from native results, not downstream scoring labels. Do not assert a fixed outcome; an abstention/null result is scientifically valid.

## Preregistered measurements
For each case/arm/timepoint: count raw observations, SHA-256 proof and source timestamps, raw source-ingested C++ edges, named-hypothesis edges **exactly zero**, selected hypothesis, epistemic status, actual DWM opposition/reopen, runtime, false observed/verified promotion, and modelworld writes. Score T0 withholding, T1 proposal coverage, T1 agreement with **internal controller** cause, wrong T1 proposals, actual wrong-belief correction (requires an earlier different proposal), and V13 mapped-minus-V14 neutral candidate selection. Counterfactual comparison does **not** isolate intelligence from graph input representation; we will explicitly report this caveat.

## P0 constraints and controls
- Controller oracle is **never read during V14 source parsing or native reasoning**. Repair T2 is **excluded** from inference. No external authenticator, no externally independently adjudicated truth, no weight learning, no unseen-case transfer.
- Quarantine data with `EdgeOrigin::Hypothetical`; no `Observed` permission, even if event source says ERP or WMS.
- Fail closed on altered or duplicated events, future T2 leakage, fraudulent source authority, foreign case IDs, missing source, inconsistent raw event hashes, nonnumeric telemetry values, or adapter output containing causal/hypothesis labels.
- Real CI must compile and execute; source-integrity negative tests must pass; SHA-256 raw evidence is archived even if failed. A passing CI run is an engineering finding, **not a win in causal performance**.
- Do not modify any F1–F13 preregistrations, scores, mechanism code or source-trust metadata defaults.

## Follow-up
If V14 performance collapses without labelled relations, build and test an explicit evidence-to-mechanism **learned or generalizable adapter** with independently authored unseen events and matched direct LLM/RAG comparisons. Only evaluate feedback-driven learning after independent outcome truth is available and prior mistakes actually change subsequent predictions.
