# GrapheneDB + Kosh Kernel v1

## One clean north-star architecture

### Purpose

GrapheneDB + Kosh Kernel v1 is a self-hosted AI memory kernel for systems that need more than vector similarity. Its purpose is to preserve temporal truth, provenance, causal paths, contradiction visibility, and physical neighbourhood retrieval while avoiding premature claims about AGI or autonomous self-improvement.

The system should be built in three layers:

1. **GrapheneDB Physical Memory Engine**
2. **Kosh Kernel / Minimal HypoKosh Reasoning Layer**
3. **Deferred Dialectic and Model-World Research Layer**

The immediate product is Layer 1 + Layer 2. Layer 3 remains a research branch until benchmark evidence proves the lower layers.

---

## 1. Core Doctrine

The core doctrine comes from TheHypoKosh paper, but stripped of unnecessary research bloat.

The system must not optimise only for the nearest semantic answer. It must preserve:

- what was true and when
- how a fact was derived
- what contradicts it
- which paths are observed, inferred, reinforced, discovered, or hypothetical
- whether a shortcut is mechanistic, compressed, predictive, analogical, or causal
- when the system has insufficient evidence and should abstain

### Public positioning

Use this:

> GrapheneDB is a self-hosted AI memory server that retrieves context through semantic similarity, causal evidence, temporal validity, and physical neighbourhood expansion while preserving provenance and alternative paths.

Do not use this yet:

> GrapheneDB is an AGI memory substrate.

---

## 2. System Architecture

```text
Client SDK / CLI / MCP / Agent Runtime
        |
        v
Kosh Kernel API
        |
        |-- temporal fact handling
        |-- provenance-aware edges
        |-- path bundle retrieval
        |-- empirical / balanced / exploratory policies
        |
        v
GrapheneDB Server
        |
        |-- physical hex lattice store
        |-- dense hex index
        |-- vector index
        |-- causal/provenance edge store
        |-- temporal metadata store
        |-- WAL + recovery
        |
        v
Physical Files
        |
        |-- graphene.lattice.bin
        |-- graphene.nodeidx
        |-- graphene.vec
        |-- graphene.edge
        |-- graphene.blob
        |-- graphene.wal
```

---

## 3. GrapheneDB Physical Memory Engine

GrapheneDB is responsible for durable storage, retrieval speed, and physical memory topology.

### Responsibilities

- fixed-offset physical hex lattice storage
- dense node-to-cell index
- local neighbourhood expansion
- vector similarity search
- causal/provenance edge traversal
- temporal filtering
- restart/crash recovery
- backup, compact, inspect, validate

### Hex lattice model

Use axial coordinates:

```text
cell = (q, r, layer)
s = -q - r
hex_distance(a, b) = max(|dq|, |dr|, |ds|)
```

Dense disk size:

```text
cells(R) = 1 + 3R(R + 1)
```

The lattice is valuable only if placement is meaningful. Therefore, vNext must implement semantic-causal-temporal placement.

---

## 4. Semantic-Causal-Temporal Placement

Current spiral placement proves storage, but not meaningful locality. The next placement algorithm should make physical distance approximate memory relationship strength.

### Placement objective

```text
best_cell = argmax score(candidate_cell)
```

Where:

```text
score(cell) =
  0.35 semantic_affinity
+ 0.25 causal_affinity
+ 0.15 temporal_affinity
+ 0.10 provenance_affinity
+ 0.10 metadata_affinity
- 0.05 overcrowding_penalty
```

### Candidate generation

1. Find semantic anchors using vector search.
2. Find causal anchors from explicit edge endpoints.
3. Find temporal anchors with overlapping validity windows.
4. Find metadata anchors sharing account, service, incident, repo, source, or domain.
5. Generate empty candidate cells within radius 1-3 of these anchors.
6. Score each candidate by local neighbourhood fit.
7. Place in the highest scoring cell if above threshold.
8. Otherwise place on the frontier ring.

### Quality metrics

- local coherence
- causal locality
- temporal consistency
- contradiction visibility
- neighbour recall@k
- cluster purity
- placement stability

The key proof metric:

```text
neighbour_recall@k = relevant facts found within hex radius k / all relevant facts
```

---

## 5. Kosh Kernel / Minimal HypoKosh Layer

This layer implements the strong, practical parts of TheHypoKosh.

### Keep in v1

- TemporalFact
- EdgeOrigin
- EdgeRole
- EvidenceRef
- derived_from chains
- reinforcement state, but only as salience
- promotion/demotion rules
- path-bundle retrieval
- empirical / balanced / exploratory policies
- no-evidence abstention
- contradiction attachment

### Defer

- full recursive self-healing loop
- curiosity engine
- analogy engine as an autonomous module
- abstraction engine as autonomous module
- model-world scheduler
- implementation feedback loops
- training trace export
- AGI-adjacent public positioning

---

## 6. Data Model

### TemporalFact

```json
{
  "node_id": 123,
  "content": "Patch X introduced memory leak",
  "source": "postmortem-042",
  "ingested_at": "2026-06-08T10:00:00Z",
  "documented_at": "2026-06-08T09:30:00Z",
  "valid_from": "2026-06-08T09:00:00Z",
  "valid_until": null,
  "confidence": 0.82,
  "metadata": {
    "incident": "INC-42",
    "service": "checkout"
  }
}
```

### ProvenanceEdge

```json
{
  "edge_id": "edge-1-3",
  "from": 1,
  "to": 3,
  "type": "causal",
  "origin": "inferred",
  "role": "compressed",
  "confidence": 0.71,
  "evidence_refs": [
    {
      "source_id": "incident-log-17",
      "span": "lines 40-58",
      "observed_at": "2026-06-08T13:00:00Z"
    }
  ],
  "derived_from": ["edge-1-2", "edge-2-3"],
  "reinforcement": {
    "count": 4,
    "last_used_at": "2026-06-09T10:00:00Z",
    "salience_boost": 0.12
  },
  "promotion_status": "not_promoted"
}
```

### HyperEdge

```json
{
  "hyperedge_id": "h-1",
  "sources": [1, 2],
  "target": 3,
  "type": "joint_causality",
  "origin": "observed",
  "role": "mechanistic",
  "confidence": 0.79,
  "activation_rule": "all_sources_required"
}
```

---

## 7. Retrieval APIs

### POST /v1/facts

Stores a temporal fact and places it in the physical lattice.

### POST /v1/edges/provenance

Stores a typed edge with origin, role, evidence, derivation, and promotion status.

### POST /v1/retrieve/bundle

Returns path bundles rather than a flat ranked list.

Example response:

```json
{
  "anchor": 10,
  "policy": "balanced",
  "as_of": "2026-06-08T14:00:00Z",
  "targets": [
    {
      "target": 42,
      "paths": [
        {
          "nodes": [10, 15, 42],
          "roles": ["mechanistic", "causal"],
          "origins": ["observed", "discovered"],
          "temporal_consistent": true,
          "confidence": 0.76
        },
        {
          "nodes": [10, 42],
          "roles": ["compressed"],
          "origins": ["inferred"],
          "temporal_consistent": true,
          "confidence": 0.61
        }
      ],
      "degeneracy": 2,
      "has_contradiction": false
    }
  ]
}
```

### POST /v1/retrieve/explain

Returns the same bundle plus why each path was included or excluded.

### POST /v1/retrieve/temporal

Returns facts valid at a requested `as_of` time.

### POST /v1/admin/lattice/quality

Computes lattice placement quality metrics.

### POST /v1/admin/validate/provenance

Checks for unsafe promotion, missing evidence, invalid derived_from chains, and reinforcement leaking into truth confidence.

---

## 8. Retrieval Policies

### Empirical

Use for enterprise, compliance, incident root-cause, and safety.

Rules:

- prefer observed and discovered edges
- allow reinforced only as salience
- exclude inferred/hypothetical unless explicitly requested
- require evidence references for strong claims
- abstain when evidence is absent

### Balanced

Use for research assistants, architecture review, strategy, and executive reasoning.

Rules:

- return empirical answer
- include inferred/compressed paths separately
- label contradictions and missing evidence
- never merge shortcut with mechanism

### Exploratory

Use for ideation and hypothesis generation.

Rules:

- allow inferred, analogical, and hypothetical paths
- label them clearly
- never promote them without evidence
- keep exploratory paths outside empirical answer by default

---

## 9. Deferred Dialectic Roadmap

The Dialectical Model World work is valuable but must remain outside the v1 kernel.

### Later modules

- ConvergentEngine
- OppositionEngine
- DialecticController
- ModelWorld node schema
- ModelWorld scheduler
- implementation/outcome feedback loop
- Kosh-aware training trace export

### Rule

Convergence is a view. It must never mutate or delete the original FiberBundle.

### Later metrics

- path-loss rate
- opposition survival rate
- compression gain
- no-evidence abstention accuracy
- false-promotion rate

---

## 10. Benchmark Plan

The benchmark must prove value over baselines, not just show architectural elegance.

### Baselines

- Graphene vector-only
- Graphene causal-only
- Graphene spiral placement
- Graphene semantic-causal-temporal placement
- Postgres + pgvector
- Qdrant
- graph-only baseline

### Workloads

1. Incident/root-cause memory
2. Customer/account memory
3. Coding/project memory
4. Policy/decision history

### Required benchmark families

- temporal supersession
- contradiction preservation
- mechanistic vs compressed path
- reinforcement without self-deception
- no-evidence abstention
- discovery promotion
- theoretical/exploratory escape

### Metrics

- recall@k
- neighbour_recall@k
- temporal accuracy
- contradiction recall
- causal path completeness
- alternative path diversity
- provenance calibration
- false-promotion rate
- path-loss rate
- p50/p95 retrieval latency
- restart/crash recovery correctness

---

## 11. Product Roadmap

### Sprint 1 — Placement Engine

- add `PlacementPolicy`
- implement semantic-causal-temporal placement
- add lattice quality endpoint
- add placement benchmark

### Sprint 2 — Provenance Hardening

- strengthen evidence refs
- validate derived_from chains
- add promotion/demotion rules
- ensure reinforcement cannot alter truth confidence

### Sprint 3 — Hyperedge Support

- implement all-source activation
- expose hyperedge API
- benchmark joint-causality retrieval

### Sprint 4 — Baseline Benchmark Pack

- build benchmark corpus
- compare against vector-only and graph-only baselines
- generate reproducible report

### Sprint 5 — Public Research Release

- publish source
- publish benchmark data
- publish ablation results
- publish architecture paper
- avoid AGI claims

---

## 12. Decision Rule

Continue investing if the next benchmark proves at least one of these:

1. Semantic-causal-temporal placement improves neighbour recall over spiral placement.
2. Bundle retrieval improves causal path completeness over vector-only retrieval.
3. Provenance-aware retrieval reduces false promotion versus ordinary graph/vector retrieval.
4. Temporal filtering reduces supersession errors without materially hurting recall.

Stop or simplify if none of these are true.

---

## Final Position

The immediate product is not a cognitive runtime and not an AGI substrate.

The immediate product is:

> a physical, provenance-aware AI memory server with temporal-causal path-bundle retrieval and mathematically meaningful lattice placement.

That is the cleanest and most defensible v1.
