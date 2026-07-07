# GrapheneDB GA Readiness Run

- timestamp: 20260707-142146
- config: Release
- profile_label: release-candidate-smoke
- approved_host: False
- ctest_exclude: graphenedb_(c_api|rc_(crash|fuzz|kosh_adapter|stress|1m_storage|soak))_tests
- focused_regex: graphenedb_(acid_lattice|lattice)_tests|graphenedb_rc5_(crash_matrix|fault_injection)_tests
- vector_index_recall_kind: auto
- vector_index_recall_min: 0.999
- vector_index_recall_nodes: 1000
- vector_index_recall_queries: 20
- vector_index_recall_dim: 16
- vector_index_recall_k: 5
- extraction_docs: 3
- extraction_nodes_per_doc: 5
- extraction_queries: 2
- extraction_vector_index: auto
- storage_nodes: 500
- storage_queries: 5
- dim: 16
- storage_vector_index: auto
- graphenedb_use_faiss: False
- extraction_thresholds: 
- storage_thresholds: 

## host profile
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\00-host-profile.log`
- status: PASS

## configure
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\01-configure.log`
- status: PASS

## build
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\02-build.log`
- status: PASS

## ctest runnable suite
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\03-ctest.log`
- status: PASS

## focused acid/crash gates
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\04-acid-crash.log`
- status: PASS

## recovery rehearsal
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\04b-recovery-rehearsal.log`
- status: PASS

## kosh adapter gate
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\04c-kosh-adapter.log`
- status: PASS

## vector index recall benchmark
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\06-vector-index-recall.log`
- status: PASS

- vector_index_recall_requested: auto
- vector_index_recall: kdtree
- vector_index_recall_mean_recall_at_k: 1

## extraction ingest benchmark
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\07-extraction-bench.log`
- status: PASS

- extraction_vector_index_requested: auto
- extraction_vector_index: kdtree

## storage retrieval benchmark
log: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-142146\08-storage-bench.log`
- status: PASS

- storage_vector_index_requested: auto
- storage_vector_index: kdtree

# Final Status

PASS
