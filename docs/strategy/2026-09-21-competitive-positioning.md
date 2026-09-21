# GrapheneDB competitive positioning — 2026-09-21

## Decision

Do **not** position GrapheneDB as another “agent memory database”. That category is already crowded and increasingly convergent around persistence, provenance, temporal history, governance, MCP, causal links and deterministic retrieval.

GrapheneDB's defensible wedge should be narrower and harder:

> **Persistent epistemic reasoning for agents that must decide under changing, contradictory evidence.**

Public product story:

- **GrapheneDB** — persistent epistemic reasoning substrate.
- **HypoKosh** — competing-hypothesis runtime: preserve, rank and revisit explanations rather than collapsing immediately to one answer.
- **Dialectical Model Worlds (DWM)** — challenge/reopen/synthesis loop: test a current world model against contradiction, intervention and new evidence.

The benchmark and flagship demo must therefore measure **decision behaviour under epistemic stress**, not generic memory recall.

## External market evidence

Fresh public review on 2026-09-21 shows a rapidly filling agent-memory category:

| System | Public positioning | Overlap with Graphene | Implication |
|---|---|---|---|
| RelataDB | governed temporal knowledge engine; provenance; time-travel; agent memory | provenance, temporal knowledge, justification | Do not lead with persistence/provenance alone. |
| XMDB | proof-of-provenance substrate; append-only history; belief tracking; point-in-time queries | provenance, durable beliefs, auditability | “Belief tracking” is no longer distinctive by itself. |
| Caura | persistent/shared/governed agent memory; provenance; supersede-don't-delete | shared memory, provenance, corrections | Governance + supersession are table stakes in this segment. |
| MADB | causal memory; current truth; causal lineage; reconsolidation; uncertainty/metamemory | causal memory, uncertainty, correction | “Causal memory” alone is also becoming a category claim. |
| Statewave | immutable episodes; typed memories; provenance; temporal validity; conflict resolution | provenance, temporal validity, conflicts | Conflict resolution needs a stronger Graphene-specific semantics. |
| PLUR | portable agent memory; corrections; benchmarked recall | persistent learning/memory | Recall benchmarks are useful but insufficient for Graphene's thesis. |
| agidb | cognitive substrate with first-class goals/beliefs; no LLM in read path | beliefs, deterministic substrate | “Cognitive substrate” language is contested; empirical differentiation matters. |
| Graphnosis | deterministic graph memory with contradictions and temporal decay | contradictions, graph memory | Contradictions as stored graph edges are not enough to differentiate. |

Research direction reinforces the opportunity for Graphene's narrower thesis:

- **ZendoWorld (2026)** reports that prediction accuracy does not imply recovery of the underlying rule and that VLM agents can propose near-uninformative experiments. This supports evaluating hypothesis identification and discriminating-test selection rather than only final labels.
- **AHOIS / Socratic agents (2026)** explicitly frames epistemic autonomy as constructing, challenging and revising explanations in response to evidence; its ablations report gains from Socratic interrogation. This validates the research direction but also means Graphene must demonstrate what persistence/runtime semantics add beyond an LLM critic loop.
- **VERITAS (2026)** uses auditable evidence labels (Supported/Refuted/Underpowered/Invalid), reinforcing that inspectable epistemic state is becoming an important systems property.
- **MAGMA (2026)** separates semantic, temporal, causal and entity graphs and reports transparent reasoning paths, increasing pressure on Graphene to prove more than multi-graph representation.

## Strategic wedge

Graphene should make claims only where GJ-Eval or another reproducible benchmark can test them.

### Claim family A — resistance to false convergence

Can Graphene preserve plausible minority hypotheses when evidence is correlated, duplicated, misleading or incomplete, instead of prematurely collapsing to the apparent majority?

Required metrics:
- premature-convergence rate,
- true-hypothesis survival by timestep,
- final false-root rate,
- governed abstention/review rate,
- selective accuracy when a root is named.

### Claim family B — explainable belief revision

When decisive contradiction or intervention evidence arrives, can Graphene reopen and revise a previously favoured explanation while preserving why the change occurred?

Required metrics:
- recovery latency after decisive evidence,
- wrong-model dwell time,
- contradiction/reopening receipt,
- provenance path from old belief -> falsifying evidence -> revised belief.

### Claim family C — evidence dependence, not evidence counting

Can Graphene avoid treating duplicate/derived/correlated reports as independent corroboration?

Required metrics:
- duplicate_swarm and correlated_sources performance,
- source-family/dependency-aware ablation,
- calibration shift under duplicated evidence.

This is currently a **known risk area**, not a marketing claim. Development evidence already suggests `duplicate_swarm` is weaker than several other variants. Treat that as an implementation target and disclose it.

### Claim family D — active epistemics

Can HypoKosh/DWM select a discriminating test that reduces uncertainty between live hypotheses, rather than merely retrieve more supporting context?

This is strategically important because ZendoWorld's findings indicate experiment selection is a real weakness for current model-based agents. GJ-Eval v1 contains `optimal_test` in the oracle but the current primary scorer does not yet establish this claim. Do not market active experiment selection until a frozen follow-on protocol evaluates it.

## What not to claim

Until independently demonstrated, avoid:

- “best agent memory database”,
- “more accurate than LLMs”,
- “solves hallucinations”,
- “causal reasoning” as a universal capability,
- “scientific reasoning engine”,
- universal superiority to Jev,
- production safety based on development-set abstention behaviour.

## Flagship demo specification

Build one reproducible incident-investigation scenario, not a gallery of demos.

Sequence:
1. initial evidence strongly favours a decoy root cause;
2. several apparent corroborators are actually derived/correlated;
3. a weak minority observation supports the true cause;
4. Graphene keeps both hypotheses live and refuses unsafe convergence;
5. DWM identifies a discriminating intervention/test;
6. the intervention falsifies the decoy;
7. Graphene reopens/reweights the model;
8. Graphene converges on the supported cause;
9. the UI/CLI displays the full belief-revision receipt.

Comparison should show the same evidence stream through at least:
- deterministic/naive evidence counting,
- direct model/Jev decision,
- Graphene full runtime.

The point is not a staged “Graphene wins” animation. The point is that an independent engineer can inspect the trace and reproduce the behavioural difference.

## Near-term operating priorities

1. Finish or reject the frozen GJ-Eval development run without changing its score-bearing semantics.
2. Run `analyze_variants.py` on the valid artifact and publish selective accuracy, false-root rate and abstention separately.
3. Treat duplicate/correlated-source weakness as a P0 research item if confirmed against Jev/ablations.
4. Add active-test-selection only as a separately frozen follow-on benchmark; do not retrofit it after seeing GJ-Eval v1 scores.
5. Make the README lead with the epistemic-decision problem, not database feature inventory.
6. Use external competitors as category evidence, not as targets for unsupported comparison claims.

## Success signal

The positioning is working when an independent engineer can accurately summarize Graphene as something like:

> “It keeps competing explanations and their evidence histories alive, knows when evidence is dependent or contradictory, and can show why it refused, reopened or changed a decision.”

That is more defensible than “memory for agents” and directly maps to falsifiable engineering behaviour.
