# Graphene uniqueness demo

GrapheneDB is not unique because it has vectors, graph edges, a WAL, or an embedded C++ library. Those are table stakes.

Its intended uniqueness is the **combined retrieval primitive**:

```text
memory node
+ embedding vector
+ causal/contradiction/supersession edge
+ signature plane
+ snapshot version
= explainable memory bundle
```

## Three retrieval styles

The demo in `examples/graphene_uniqueness_demo.cpp` creates the same small dataset and shows three outputs.

### 1. Vector-only style

Finds semantically similar chunks. This is useful, but it can mix:

- current evidence
- stale hypotheses
- similar-but-different incidents
- contradictory memories

### 2. Graph-only style

Shows known links between nodes, but does not know which nodes are semantically relevant to the user's query.

### 3. Graphene causal-memory style

Returns a `MemoryBundle` with:

- semantic candidates
- signature-plane candidate reduction
- causal path from root to anchor memory
- confidence score
- contradiction awareness
- snapshot version
- reason codes

## Run it

```bash
./scripts/run_graphene_uniqueness_demo.sh
```

Expected shape:

```text
=== Same question, three retrieval styles ===
Question: Why did checkout fail after the GCP migration?

1) Vector-only style
  score=... node=... :: INC-1421...
  score=... node=... :: Old note...
  score=... node=... :: INC-1440...

2) Graph-only style
  Known link path: root ADR -> deployment -> incident symptom
  But graph-only does not rank semantic closeness or know query intent.

3) Graphene causal-memory style
  target_root=... confidence=... paths=...
  why=semantic similarity to candidate memories
  why=signature-plane candidate reduction
  why=causal path from root memory to anchor memory
```

## Product claim that is fair

> GrapheneDB is an embedded causal-memory database for AI systems that returns evidence-backed memory bundles rather than only similar chunks.

## Product claim that is not yet fair

> GrapheneDB replaces Qdrant, Neo4j, or SQLite.

That is not the goal of v1.
