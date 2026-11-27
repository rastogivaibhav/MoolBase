# Independent evaluation protocol — proposal for reviewer approval

This is a separate transfer protocol, not an amendment to the six EGPM gates or their frozen metrics. Pre-register before observing new held-out results. Publish negative results. Separate engineering acceptance, retrieval, semantic extraction, governed reasoning and scientific comparison.

## Research questions

R1: compare standalone MoolBase (C) against genuine MeMo (B), conventional RAG (A) and MeMo+MoolBase (D). Do not substitute D for C. R2: test causal contribution of topology, FiberBundle, HypoKosh, dialectical opposition and stability critic. R3: test whether ontology helps extraction and governed decisions. NVIDIA Auto Ontology is an optional research candidate, not a shipped integration.

## Lock before inference

Use original MeMo MuSiQue manifest and full corpus, authoritative aliases; record SHA-256 of dataset, document manifest, selected IDs and source bytes. Eight EGPM development questions permanently excluded from held-out. Use original frozen EGPM held-out/confirmation IDs when applicable; no changes to those criteria. For new tests select disjoint blinded workloads, stratify by question type/hop count and predeclare sampling and power. Keep answer labels sealed until prediction files are hashed. No tuning on confirmation. NarrativeQA/BrowseComp+ are separate replication/adaptation tracks.

A: matched text RAG; B: released MeMo checkpoint with documented recall procedure; C: native MoolBase retrieval and runtime (no MeMo); D: same C plus MeMo recall. Same executive weights, decoding, document corpus and inference access constraints. Record tokens, calls, wall time, model loading/index construction, RAM and disk. Distinguish oracle gold-document/annotation diagnostics from ordinary inference; no gold answer nodes in non-oracle arms. Same source budgets and controls where meaningful; report deviations explicitly, including parametric training data access and memory build cost. Compare verifier-only, static/active retrieval, native/Python graph parity and ordinary graph topology. Preserve exact spans/hashes and receipts; independently adjudicate semantic correctness and classify missing/ambiguous/contradictory outcomes.

## Statistical plan

Primary transfer comparison C vs B (and C vs A), secondary D vs B and D vs C. Report normalized EM, best-alias token F1, coverage, wrong-answer rate per all questions, selective accuracy, paired stratified bootstrap 95% intervals, one-sided exact McNemar p-values and Holm correction for the specified family; scorer provides point estimates, intervals and p-values, not automatic success. Predefine superiority criteria and, if claiming competitiveness, a scientifically justified non-inferiority margin and power before inference. Failure to reject a difference is not proof of equivalence. EGPM G5/G6 still require their original superiority criteria. Report original semantic judge separately if available, with version and budget; exact/F1 not the original paper metric.

## Input contract

Gold JSONL: one object per question: {"id":"q1","stratum":"2hop","aliases":["answer","alternate"]}. Gold stays with independent scorer. Prediction JSONL: {"id":"q1","answer":"answer","seconds":0.2,"model_calls":1,"prompt_tokens":50,"completion_tokens":4}; use JSON null for abstention, not guessed answer. Costs are per-question inference only, nonnegative finite, integer counts. Store sidecar provenance, source docs, receipts, raw outputs and build costs separately, linked by ID. Scorer rejects duplicates, missing/extraneous IDs, invalid counts, non-finite values and mismatched gold or ID digest. It cannot authenticate externally supplied costs or semantic evidence; audit separately.

```sh
python3 evaluation/transfer/score.py --gold gold.jsonl --gold-sha256 GOLD_HASH --ids-sha256 IDS_HASH \
  --pred A=rag.jsonl --pred B=memo.jsonl --pred C=moolbase.jsonl --pred D=combined.jsonl --out score.json
```

ID digest: sorted unique IDs joined by LF with terminal LF, UTF-8 SHA-256. Gold hash is raw file SHA-256. Pin both before inference. Never reveal the gold file to prediction adapters. Run `python3 evaluation/transfer/score.py --self-test` first.

## Component experiments

Use identical evidence and budgets, full system minus each component, plus simpler conventional baselines. Lattice: ordinary adjacency with same edges and search limits. FiberBundle: flat evidence set with same provenance. HypoKosh: fixed candidate rank. DWM: no opposition/reopen. Critic: disabled vs enabled; measure wrong stable conclusions, oscillation and premature commitments, not stability alone. Ontology: same extraction model and documents, no ontology vs frozen ontology; measure relation precision/recall and downstream QA. Require reviewer-verified toggles map to actual implementation; unsupported toggles are blockers, not simulated results. Add temporal updates, correlated copies, adversarial unsupported sources and contradiction tests. Report each separately. No component results claimed by this protocol.
