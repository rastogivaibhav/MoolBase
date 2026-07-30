# Lattice Retrieval

Lattice-aware retrieval extends `causal_search()` with a graphene-inspired propagation score.

## Retrieval flow

The existing causal-memory flow remains intact:

```text
signature-plane candidates
+ vector anchor score
+ reverse causal path to root
+ contradiction and ambiguity checks
```

When `DBOptions::tuning.enable_lattice_retrieval` is true, GrapheneDB also:

1. Seeds lattice activation from the inspected semantic anchors.
2. Traverses durable lattice bonds up to `lattice_max_hops`.
3. Applies decay, bond strength, defect penalty, and cross-layer penalty.
4. Records the best propagated activation as `MemoryBundle::lattice_score`.
5. Blends the lattice score into bundle confidence using `lattice_weight`.

The returned bundle includes:

- `lattice_score`
- `lattice_neighbors`
- `lattice_explanation`
- `why_retrieved` reason: lattice propagation through graphene-inspired neighbor bonds

## Tuning

Relevant `RetrievalTuning` fields:

```cpp
bool enable_lattice_retrieval;
uint32_t lattice_max_hops;
double lattice_decay;
double lattice_defect_penalty;
double lattice_cross_layer_penalty;
double lattice_weight;
```

Defaults keep lattice retrieval disabled for backward compatibility. New lattice-focused applications should enable it explicitly and can require coordinates with `DBOptions::require_lattice`.

## Interpretation

The lattice score is a memory-topology signal. It means a candidate is supported by nearby validated lattice bonds. It does not mean the system computed a physical graphene conductivity or material property.
