# GJ-Eval v1: GrapheneDB vs Jev Epistemic Decision Benchmark

Status: candidate protocol frozen before comparative test scores; final Gate-0 freeze occurs after harness self-review and adapter-contract conformance.
Scope: reasoning/evaluation only. No GrapheneDB durable-format changes.
Primary systems: Jev-Structured and GrapheneDB Full Runtime.
Secondary systems: Jev-Raw and GrapheneDB ablations.

## 1. Research question

GJ-Eval tests one falsifiable question:

> Does a persistent epistemic world model with source-lineage-aware evidence, competing hypotheses, stability criticism, dialectical opposition/reopening, and governed projection improve machine decisions under contradictory and changing evidence compared with direct probabilistic decision inference over the same canonical evidence?

This is not a database throughput benchmark and not a claim that either architecture is universally better.

## 2. Scientific commitments

1. Freeze task generation, schemas, metrics, budgets, adapters, and result interpretation before the hidden test split is scored.
2. Give both systems the same canonical observations and the same decision choices.
3. Jev-Structured receives provenance, source-family, dependency, contradiction, supersession, confidence, and temporal fields when Graphene receives them. This prevents input asymmetry from masquerading as a reasoning advantage.
4. Graphene receives evidence incrementally through its persistent state. Jev receives the complete canonical state visible at the same timestep. State-transfer bytes and inference latency are recorded separately.
5. The shared extractor is outside the comparison in v1. Raw-text extraction is a later benchmark.
6. Negative results, failed tracks, adapter limitations, and unsupported metrics are reported rather than silently dropped.
7. No benchmark tuning is allowed on the hidden test split.

## 3. Benchmark arms

### J0 - Jev Raw
Receives the human-readable observation state and the fixed decision schema.

### J1 - Jev Structured (primary Jev arm)
Receives the same observations plus canonical evidence metadata:
- evidence_id
- source_id
- source_family
- derived_from
- relation role and origin
- confidence
- observed_at
- valid_from / valid_until when present
- supersedes / contradicts relationships
- candidate hypotheses and permitted actions/tests

The Jev adapter must use TypeSafe's recommended decomposition into narrow typed judgments where practical. It must not collapse the whole workflow into one free-form prompt merely to make Jev weaker.

### G0 - deterministic evidence baseline
No HypoKosh/dialectical reasoning. Counts/aggregates canonical evidence with a pre-registered deterministic rule. This is a harness baseline, not a Graphene product claim.

### G1 - Graphene causal traversal
Graphene evidence/model-world traversal without dialectical reopening.

### G2 - G1 + FiberBundle independence semantics
Adds source-family/dependency-aware support accounting.

### G3 - G2 + HypoKosh hypothesis retention
Adds competing hypothesis generation/ranking where the current runtime supports it.

### G4 - G3 + Stability/Lyapunov critic
Adds epistemic admissibility and premature-convergence detection.

### G5 - G4 + opposition/reopening
Adds dialectical challenge, targeted re-expansion, and revised convergence.

### G6 - Full Graphene runtime (primary Graphene arm)
Uses the complete supported Graphene -> FiberBundle -> HypoKosh -> stability critic -> opposition/reopening -> governed projection path.

If an ablation cannot be cleanly expressed by the current runtime without changing its semantics, that arm is marked NOT SCORED until a dedicated native benchmark runner exists.

## 4. Tracks

### Track A - System-One parity
One-shot typed decisions with clean independent evidence.

### Track B - Epistemic stress
Paired worlds are transformed by exactly one controlled manipulation:
- duplicate_swarm
- correlated_sources
- false_majority
- late_contradiction
- source_invalidation
- supersession
- missing_evidence
- misleading_chronology
- minority_truth
- interaction_effect
- model_reversal

### Track C - Hypothesis survival
The true hypothesis begins weak while a plausible false hypothesis dominates early evidence.
Primary endpoint: True Hypothesis Survival@K before decisive evidence arrives.

### Track D - Dialectical recovery
The current leading model becomes false after later evidence/intervention.
Primary endpoints: wrong-model dwell time and recovery latency.

### Track E - Active investigation
The system may request one bounded observation/intervention from a fixed menu.
Primary endpoint: information gained per unit test cost and time-to-identification.

### Track F - Longitudinal world state
Evidence arrives across a long episode with supersession and invalidation.
Primary endpoints: stale-belief rate, historical consistency, and cumulative state-transfer cost.

## 5. WorldShift synthetic worlds

Each world contains a hidden ground-truth causal model and a public sequence of canonical observations.

Example hidden truth:

database_saturation -> retry_amplification -> checkout_failure

A deployment can occur nearby in time without being causal. At timestep t the model sees only evidence released at or before t.

Required episode phases:
- T0 ambiguous symptom
- T1 plausible but potentially misleading lead
- T2 additional support, sometimes correlated/duplicated
- T3 contradiction or discriminating evidence
- T4 independent evidence
- T5 optional intervention
- T6 intervention outcome
- T7 final decisive evidence

The oracle is never sent to either system.

## 6. Common decision contract

At every scored timestep, an adapter emits a Prediction v1 object.

Required:
- world_id
- timestep
- system
- root_choice
- act: act | abstain | review
- selected_confidence in [0,1]
- latency_ms

Optional when supported:
- choice_probabilities
- ranked_hypotheses
- epistemic_status
- requested_test
- receipt
- input_bytes
- output_bytes
- provider_cost

Timeouts, malformed responses, and adapter errors remain in the denominator and can never receive correct-abstention credit.\n\nA system is never assigned a fabricated probability distribution. Metrics requiring a full distribution are reported only for systems that actually emit one.

## 7. Primary metrics

### Final Accuracy
Correct root at the final scored timestep.

### Trajectory Accuracy
Fraction of scored timesteps with the correct root.

### Wrong-Model Dwell Time
Number of post-contradiction timesteps spent committed to a false root before correction/abstention. Lower is better.

### Recovery Latency
Number of new evidence events after the first decisive contradiction until the system reaches the correct root or a correct abstention state.

### Premature Convergence Rate
Fraction of episodes in which the system commits above the pre-registered confidence threshold to a false root before the oracle marks evidence sufficient.

### Contradiction Response Delta
Change in confidence assigned to the contradicted hypothesis immediately before vs after the contradiction.

### Duplicate Inflation Delta
Confidence change caused only by duplicated/correlated evidence.

### True Hypothesis Survival@K
Fraction of pre-decisive timesteps for which the hidden true hypothesis remains in the system's top K hypotheses.

### Abstention Quality
Precision/recall of abstention/review against oracle-defined insufficient-evidence states.

### Calibration
ECE on selected-decision confidence. Full multiclass Brier score is computed only where choice_probabilities are genuinely available.

### Experiment Efficiency
Information gain divided by declared intervention/test cost.

### Historical Consistency / Stale Belief Rate
Whether historical queries exclude future evidence and superseded/invalidated beliefs remain incorrectly active.

### Systems metrics
Latency p50/p95/p99, throughput, input bytes/tokens where available, output bytes, provider cost, persistent-state update bytes.

## 8. Primary statistical comparison

Primary comparison:
G6 Full Graphene vs J1 Jev Structured on paired hidden-test episodes.

Primary endpoint family:
1. Wrong-Model Dwell Time
2. Recovery Latency
3. Premature Convergence Rate
4. Final Accuracy

Report paired bootstrap 95% confidence intervals over worlds.

A measurable advantage may be claimed for an endpoint only when:
- the pre-registered direction is satisfied,
- the paired 95% CI excludes zero,
- the metric was supported by both adapters,
- no post-hoc task filtering was used.

No universal winner score is permitted. Results are reported per track and metric.

## 9. Splits and anti-benchmaxxing

Development: visible worlds for adapter correctness and debugging.
Validation: visible after protocol freeze; may be used for one adapter-conformance pass.
Test: generated with a secret seed supplied only at the final run.

The test seed is not committed. The final receipt stores only SHA-256(seed) plus generator commit SHA.

Once test scoring starts:
- no code, prompts, thresholds, questions, or task templates may change;
- failed worlds remain in the denominator unless a pre-registered exclusion rule applies;
- infrastructure reruns preserve exact adapter/model versions and are logged.

## 10. Adapter fairness

### Jev
The repository does not invent or pin a private Jev SDK surface. The Jev adapter is an external command/process conforming to Prediction v1. This permits the official current TypeSafe client to be used without changing the benchmark protocol.

The Jev implementation must record model/version, workflow-definition hash, state serialization hash, timestamps, and provider latency/cost where exposed.

### Graphene
Preferred v1 integration:
1. ingest canonical nodes/relations through POST /v1/extractions;
2. call POST /v1/reason/runtime for full runtime decisions;
3. use external_to_node_id from extraction response to map primary_node to hypothesis IDs;
4. use dedicated HypoKosh/dialectic surfaces where a track requires ranked hypotheses or discriminating tests;
5. preserve compact receipt and stability/Lyapunov fields in the run artifact.

Graphene durable data is isolated per benchmark run.

## 11. Acceptance gates before hidden test

GATE-0 Protocol freeze:
- manifest validates
- schemas frozen
- metric formulas frozen
- exclusion rules frozen

GATE-1 Generator determinism:
- same seed -> byte-identical world IDs/oracles
- different seed -> no duplicate world IDs
- stress transformations change only declared fields

GATE-2 Adapter conformance:
- both primary adapters pass 100 canonical contract cases
- malformed/timeout responses are retained as failures
- no oracle field is visible to adapters

GATE-3 Graphene non-regression:
- existing Graphene CTest/runtime contracts pass unchanged
- benchmark adds no durable-format mutation

GATE-4 Paired pilot:
- at least 250 development episodes complete for J1 and G6
- no score-driven protocol changes after this gate

GATE-5 Hidden test:
- secret seed injected
- manifest + code commit hashes recorded
- one canonical run per arm
- statistical report generated automatically

## 12. Result interpretation

Valid outcomes include:
- Jev dominates one-shot decisions while Graphene improves recovery under evidence shifts.
- Graphene matches static decisions and improves epistemic stress metrics.
- Graphene's extra reasoning adds cost without measurable reliability benefit.
- Jev remains competitive even with adversarial evidence, falsifying part of the Graphene thesis.
- Different tracks favour different architectures.

All are scientifically useful.

## 13. Explicit v1 non-goals

- raw-text extraction quality
- general LLM generation quality
- proving global Lyapunov stability
- unrestricted autonomous experiment execution
- medical/legal high-stakes deployment claims
- public state-of-the-art claims
- changing GrapheneDB storage semantics to improve the benchmark

## 14. Repository layout

benchmarks/gj_eval/
  README.md
  manifest.v1.json
  schemas/
    world.v1.schema.json
    prediction.v1.schema.json
  generate_worldshift.py
  score.py
  adapters/
  runs/

docs/benchmarks/GJ_EVAL_V1_SPEC.md

## 15. Implementation sequence

P0. Commit the candidate protocol and schemas; complete harness self-review before final Gate-0 freeze.
P1. Implement deterministic WorldShift generator and self-tests.
P2. Implement Graphene HTTP adapter against current runtime.
P3. Implement Jev adapter using the official current TypeSafe early-access API through the command contract.
P4. Add G0-G6 native ablation runner where clean runtime switches do not already exist.
P5. Run development conformance only.
P6. Freeze adapter hashes.
P7. Execute hidden paired test once.
P8. Publish report with caveats, failures, full ablation table, and raw result artifacts.

The benchmark exists to falsify or support the Graphene thesis, not to manufacture a leaderboard win.
