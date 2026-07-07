# GrapheneDB Enterprise GA Campaign

- timestamp: 20260705-170517
- config: Release
- build_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\build-enterprise-ga
- profile_label: utf8-summary-smoke-14
- approved_host: False
- soak_seconds: 300
- soak_dim: 32
- stress_incidents: 4
- stress_queries: 1
- stress_dim: 8
- stress_vector_index: auto
- one_m_nodes: 20
- one_m_queries: 1
- one_m_dim: 8
- one_m_vector_index: auto
- fuzz_runs: 10000
- graphenedb_use_faiss: False
- skip_soak: True
- skip_ga_readiness: True
- skip_full_ctest: True
- skip_fuzz: True

## host profile
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\00-host-profile.log`
- status: PASS

## configure
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\01-configure.log`
- status: PASS

## build
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\02-build.log`
- status: PASS

## full ctest suite
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\03-ctest.log`
- status: SKIP
- note: skipped by caller

## ga readiness harness
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\04-ga-readiness.log`
- status: SKIP
- note: skipped by caller

## 100k stress profile
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\05-100k-stress.log`
- status: PASS

- 100k_vector_index_requested: auto
- 100k_vector_index: kdtree
- ingest_nodes_per_sec: 9886.31
- causal_hit_rate: 1
- causal_p95_ms: 0.2682
- metadata_service_p95_ms: 0.0023
- cross_layer_edges: 4
- defect_edges: 4
- synthetic_edges: 3
- reopen_ms: 4.6077

## 1m storage profile
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\06-1m-storage.log`
- status: PASS

- 1m_vector_index_requested: auto
- 1m_vector_index: kdtree
- vector_p95_ms: 0.0214
- causal_lattice_p95_ms: 0.3152
- causal_root_hit_rate: 0
- metadata_service_p95_ms: 0.0076
- cross_layer_edges: 3
- defect_edges: 3
- synthetic_edges: 2
- reopen_ms: 3.1448

## soak gate
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\07-soak.log`
- status: SKIP
- note: skipped by caller

## real filesystem failure gates
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\08-filesystem.log`
- status: SKIP
- note: executable not found: graphenedb_real_filesystem_failure_tests under C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\build-enterprise-ga

## disk pressure gates
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\09-disk-pressure.log`
- status: SKIP
- note: executable not found: graphenedb_disk_pressure_tests under C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\build-enterprise-ga

## coverage fuzz gate
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\10-fuzz.log`
- status: SKIP
- note: skipped by caller

- full_ctest_completed: False
- ga_readiness_completed: False
- fuzz_completed: False
- soak_completed: False
- filesystem_gate_passed: False
- disk_pressure_gate_passed: False
- target_scale_dimensions_ready: False
- full_day_soak_profile_ready: False
- release_like_profile_ready: False

# Final Status

PARTIAL
