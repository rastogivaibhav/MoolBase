# Physical Lattice Storage Preview

This branch adds an opt-in physical lattice materialization mode to GrapheneDB.

## What changed

`DBOptions::physical_lattice_storage = true` now writes a deterministic `graphene.lattice` sidecar file. The file stores visible lattice nodes as physical hex cells rather than treating the lattice coordinate only as node metadata.

Each `CELL` frame is ordered by:

1. `layer`
2. hex ring distance from the origin
3. `q`
4. `r`
5. `node_id`

Each cell stores:

```text
CELL <layer> <q> <r> <node_id> <n0> <n1> <n2> <n3> <n4> <n5> <lattice_edge_ids>
```

The six neighbour slots map to axial hex directions:

```text
(+1,0), (+1,-1), (0,-1), (-1,0), (-1,+1), (0,+1)
```

A missing neighbour is stored as `-1`.

## CLI

Use `--physical-lattice` on commands that open the database:

```bash
graphenedb_cli init ./db 2 --physical-lattice
graphenedb_cli put-node ./db 2 center 1,0 1 root --lattice 0,0,0 --physical-lattice
graphenedb_cli put-node ./db 2 east 0.9,0.1 1 symptom --lattice 1,0,0 --physical-lattice
graphenedb_cli put-edge ./db 2 0 1 supports --bond sigma --coupling same-layer --physical-lattice
graphenedb_cli compact ./db 2 --physical-lattice
```

## Validation

`validate()` checks that the physical lattice file exists and that its `CELL` count matches the visible lattice node count.

## Honest boundary

The canonical recovery source is still `graphene.data` + `graphene.wal`. This preview adds a physical hex-cell ordered materialization layer and sidecar layout. A later GA-grade lattice-native engine would make the lattice file/page layout the primary source of truth and perform traversal directly against lattice pages.
