# GrapheneDB Preview Hardware Profile

- timestamp: 20260707-141656
- config: Release
- build_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\build-preview-profile
- profile_label: developer-preview
- intended_hardware: True
- graphenedb_use_faiss: False
- vector_baseline: incidents=5000 queries=200 dim=64
- vector_index: nodes=5000 queries=200 dim=32 k=10 index=auto min_recall=0.999
- extraction: docs=100 nodes_per_doc=50 queries=100 dim=64 index=auto
- storage: nodes=20000 queries=100 dim=64 index=auto

## host profile
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\00-host-profile.log`
- status: PASS

## configure
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\01-configure.log`
- status: PASS

## build benchmark targets
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\02-build.log`
- status: PASS

## vector baseline comparison
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\03-vector-baseline.log`
- status: PASS

- vector_root_hit_rate: 0
- causal_root_hit_rate: 1
- causal_p95_ms: 0.61

## vector index recall
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\04-vector-index.log`
- status: PASS

- mean_recall_at_k: 1
- worst_recall_at_k: 1
- vector_index_resolved: kdtree

## extraction ingest performance
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\05-extraction.log`
- status: PASS

- extraction_vector_index_requested: auto
- extraction_vector_index: flat
- extract_nodes_per_sec: 2510.66
- extract_doc_p95_ms: 34.0182
- causal_lattice_p95_ms: 0.1945
- causal_root_hit_rate: 0.98
- metadata_doc_p95_ms: 0.0081
- avg_lattice_neighbors: 4
- reopen_ms: 225.275

## storage retrieval performance
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\06-storage.log`
- status: PASS

- storage_vector_index_requested: auto
- storage_vector_index: flat
- ingest_nodes_per_sec: 260.502
- vector_p95_ms: 3.9174
- causal_lattice_p95_ms: 0.8917
- causal_root_hit_rate: 0.98
- metadata_service_p95_ms: 0.2727
- avg_lattice_neighbors: 3
- cross_layer_edges: 3333
- reopen_ms: 817.892

# Final Status

PASS
