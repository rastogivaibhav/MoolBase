# Graphene Lattice Model

GrapheneDB implements a graphene-inspired hexagonal lattice memory model. The model is a database topology and retrieval primitive, not a material-science simulator.

## What is implemented

- Hexagonal axial coordinates on nodes: `q`, `r`, and `layer`.
- Durable bond metadata on edges: bond type, defect type, layer coupling, and bond strength.
- Lattice validation for same-layer and cross-layer bonds.
- Optional `DBOptions::require_lattice` mode that rejects nodes without coordinates.
- Optional lattice-aware retrieval that propagates activation through lattice bonds and blends it into `MemoryBundle` confidence.
- Atomic batch ingestion with `BatchInput`/`BatchResult`.
- Deterministic extraction/placement helper functions in `graphene/lattice_placement.hpp`.

## Coordinate system

Nodes may carry:

```cpp
struct LatticeCoord {
  int32_t q;
  int32_t r;
  int32_t layer;
};
```

Same-layer hex neighbors are exactly:

```text
(+1,  0)
(+1, -1)
( 0, -1)
(-1,  0)
(-1, +1)
( 0, +1)
```

The `layer` field supports monolayer and multilayer memory layouts. Cross-layer bonds require an explicit non-`None` coupling.

## Bonds and defects

GrapheneDB stores these bond types:

- `None`
- `Sigma`
- `Pi`
- `VanDerWaals`
- `Defect`
- `Synthetic`

`Sigma` and `Pi` bonds must connect same-layer hex-neighbor coordinates. `VanDerWaals` bonds must connect nearby cross-layer coordinates and declare a cross-layer coupling. `Defect` and `Synthetic` bonds may bypass geometric neighbor validation, but they are explicit and can be penalized during retrieval.

Defect types include vacancy, substitution, Stone-Wales, strain, doped, and boundary markers. These are retrieval/topology annotations, not physical predictions.

## Placement and extraction

The core DB accepts explicit coordinates. For callers that need a first-pass extraction path, GrapheneDB also provides:

```cpp
enum class PlacementStrategy {
  InputOrder,
  SemanticGroups
};

LatticeCoord hex_spiral_coord(uint32_t index, int32_t layer);
Status assign_lattice_batch(std::vector<NodeInput> nodes,
                            const LatticePlacementOptions& options,
                            LatticePlacementResult* out);
```

`InputOrder` preserves caller order and assigns coordinates on a hex spiral. `SemanticGroups` first sorts memories by incident, signature, root/symptom/impact role, and content, then assigns nearby coordinates and emits stronger same-group bonds. Boundaries between groups are marked as synthetic boundary bonds.

This placement is deterministic and intentionally simple. It provides a stable ingestion baseline, not a physics-derived lattice packing algorithm, LLM extractor, or learned semantic clustering system. More advanced extractors can produce their own `NodeInput` and `EdgeInput` records and commit them with `put_batch()`.

The preferred extraction boundary is now `GrapheneDB::put_extraction()`, documented in [`EXTRACTION_INGESTION.md`](EXTRACTION_INGESTION.md). It adds source-scoped external IDs, idempotent re-import, relation resolution by external ID, and automatic lattice placement before the records are durably committed.

## Lattice inspection

Applications can inspect durable lattice topology directly:

```cpp
std::vector<uint32_t> neighbors = db.lattice_neighbors(node_id, max_hops);
```

The API respects snapshot visibility within the current uncompacted process
epoch. Deleted nodes and their incident bonds disappear from current results,
while an older snapshot can still see them until compaction. Compaction writes
only the current visible state, so historical snapshots are not durable across
compaction/reopen in this RC.

The CLI exposes the same view:

```bash
graphenedb_cli neighbors <path> <dim> <node-id> [max-hops]
```

## TSV import

For simple extraction pipelines, the CLI can import tab-separated rows and apply semantic lattice placement before committing one batch:

```bash
graphenedb_cli import-tsv <path> <dim> <tsv-file>
```

The TSV format is:

```text
content<TAB>vector_csv<TAB>signature<TAB>incident<TAB>role
```

`role` may be `root`, `symptom`, `impact`, or `node`. The importer assigns lattice coordinates with `PlacementStrategy::SemanticGroups`, creates group-local lattice bonds, and commits the batch with `put_batch()`.

## What is not implemented

GrapheneDB does not simulate:

- carbon atoms
- molecular dynamics
- electron transport
- tight-binding Hamiltonians
- Dirac cone behavior
- physical conductivity
- material strength, doping, or strain physics

The analogy stops at graph topology, layer-aware bonds, defects, and propagation-style retrieval.
