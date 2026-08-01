# Related systems and product positioning

This document supports the paper’s related-work section and the public developer alpha. It is not a performance ranking. It identifies category boundaries and the evidence required before making comparative claims.

## Primary category

GrapheneDB is best described as an **embedded evidence database and epistemic-control runtime for agentic systems**.

Its primary decision is not merely “which records are similar?” or “which graph pattern matches?” but:

```text
Is the current evidence independently supported, complete, temporally valid,
contradiction-safe and sufficient for answer, deeper search, contestation or abstention?
```

## Category comparison

| Category | Primary job | Representative systems/research | GrapheneDB relationship | Current comparative claim |
|---|---|---|---|---|
| Vector database | Retrieve nearest or hybrid-matching records at scale | Qdrant, Milvus, Weaviate, FAISS | Can provide upstream candidates or be replaced by GrapheneDB’s local vector layer for small embedded workloads | No scale or latency superiority claim |
| Property/knowledge graph | Store and query general graph data | Neo4j and other property-graph systems | Can remain the enterprise graph while GrapheneDB evaluates selected evidence subgraphs | No query-language or ecosystem superiority claim |
| RAG | Ground generation in retrieved passages | RAG | GrapheneDB adds evidence lineage and governed resolution around retrieval | No general answer-quality superiority claim |
| GraphRAG | Use graph structure to improve retrieval and summarisation | GraphRAG | Shares graph-structured retrieval motivation; GrapheneDB focuses on admissibility, contradiction and action gating | Complementary, not a replacement claim |
| Self-reflective retrieval/generation | Retrieve, critique and regenerate | Self-RAG | GrapheneDB externalises evidence state and bounded recovery in a persistent runtime | No learned-policy superiority claim |
| Agent reasoning/action loop | Interleave reasoning and tools | ReAct | GrapheneDB can gate whether an action has sufficient evidence | Complementary control substrate |
| Reflective agent memory | Preserve feedback for later trials | Reflexion | GrapheneDB preserves evidence ancestry and governed claim status rather than only reflective text | Complementary memory semantics |
| Agent checkpoint/state store | Resume workflow and thread state | LangGraph-style persistence | GrapheneDB stores evidence and epistemic receipts, not the complete orchestration state | Different responsibility |
| Provenance/truth maintenance | Track derivation and revise beliefs | database provenance and TMS research | Closest conceptual ancestry; GrapheneDB provides an implemented agent-facing synthesis | Formal comparison remains future work |

## Why choose GrapheneDB for an alpha experiment

Choose it when the workload requires several of these simultaneously:

- local or embedded operation;
- typed temporal and causal graph paths;
- source, evidence-family and derivation lineage;
- correlated-evidence grouping;
- contradiction preservation and blocking;
- explicit evidence-required and abstention states;
- bounded targeted recovery rather than generic retry loops;
- deterministic compact decision receipts;
- an auditable separation between observed, inferred, reinforced and hypothetical relations.

## When not to choose it

Do not choose the current alpha when the primary requirement is:

- billion-scale vector retrieval;
- a mature general-purpose graph query language;
- managed cloud clustering or multi-region replication;
- production SLAs and enterprise support;
- internet-edge multi-tenant hosting;
- automatic semantic truth determination;
- a complete agent orchestration framework;
- fully validated Windows server operation.

## Recommended coexistence patterns

### External vector retrieval

```text
Vector database
  -> top-k candidate records
  -> GrapheneDB evidence graph and lineage
  -> FiberBundle
  -> answer | deepen | contest | abstain
```

### Enterprise knowledge graph

```text
Property/knowledge graph
  -> selected evidence subgraph
  -> GrapheneDB epistemic controller
  -> compact decision receipt
```

### Agent framework

```text
Agent execution/checkpoint framework
  -> GrapheneDB evidence query
  -> governed status
  -> continue | retrieve | ask human | stop
```

## Comparative benchmark plan

A publishable comparison must separate conventional database performance from epistemic-control quality.

### Conventional measurements

- insert throughput;
- p50/p95/p99 retrieval latency;
- vector recall@k;
- graph traversal latency;
- memory and disk consumption;
- restart and recovery time;
- concurrent-reader behaviour;
- package and deployment complexity.

### Epistemic-control measurements

- duplicate-evidence inflation;
- shared-ancestry detection;
- independent-corroboration recognition;
- contradiction-block recall;
- false-resolution rate;
- abstention precision and recall;
- missing-hop recovery;
- targeted versus broad retrieval work;
- opposition information yield;
- receipt size and deterministic reproduction.

## Claim policy

Until official head-to-head tests are frozen and reproduced, public material must use:

- “different primary purpose” rather than “better”;
- “controlled mechanism result” rather than “superior reasoning”;
- “complements” rather than “replaces”;
- exact workload and environment for every number;
- explicit unsupported/planned labels for unimplemented features.
