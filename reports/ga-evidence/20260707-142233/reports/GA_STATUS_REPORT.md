# GrapheneDB GA Status Report
- generated_at_utc: 2026-07-07T13:20:48.0941267Z
- local_evidence_passed: 13/13
- latest_ga_readiness_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953
- latest_ga_attempt_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953
- latest_ga_evidence_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-evidence\20260707-142046
- latest_enterprise_ga_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517
- latest_preview_hardware_dir: C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656

## Locally Evidenced
- [PASS] Latest GA readiness summary: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953\GA_READINESS_SUMMARY.md`
- [PASS] Latest GA evidence manifest: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-evidence\20260707-142046\EVIDENCE_MANIFEST.json`
- [PASS] Install package archive: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\graphenedb-install-package.zip`
- [PASS] Install package sha256: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\graphenedb-install-package.zip.sha256`
- [PASS] Install package manifest: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\graphenedb-install-package.zip.manifest.json`
- [PASS] Release candidate bundle metadata: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\RELEASE_CANDIDATE_BUNDLE_META.json`
- [PASS] Recovery rehearsal output: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\RECOVERY_REHEARSAL_OUTPUT.txt`
- [PASS] Kosh adapter gate output: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\RC_REAL_KOSH_ADAPTER_OUTPUT.txt`
- [PASS] Vector index recall output: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\VECTOR_INDEX_RECALL_OUTPUT.txt`
- [PASS] RC bundle progress report: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\GA_PROGRESS_RC_BUNDLE.md`
- [PASS] GA harness progress report: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\GA_PROGRESS_GA_HARNESS.md`
- [PASS] Filesystem failure progress report: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\GA_PROGRESS_FILESYSTEM_FAILURES.md`
- [PASS] Extraction contract progress report: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\GA_PROGRESS_EXTRACTION_CONTRACT.md`

## Enterprise Campaign
- [PARTIAL] Latest enterprise campaign summary: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\ENTERPRISE_GA_SUMMARY.md`
- [PARTIAL] Host profile: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\enterprise-ga\20260705-170517\HOST_PROFILE.json`
- approved_host: False
- profile_label: utf8-summary-smoke-14
- rich_workload_harness_present: True
- target_scale_dimensions_ready: False
- 100k_dim: 8
- 100k_vector_index_requested: auto
- 100k_vector_index: kdtree
- 100k_ingest_nodes_per_sec: 9886.31
- 100k_causal_hit_rate: 1
- 100k_causal_p95_ms: 0.2682
- 100k_cross_layer_edges: 4
- 1m_dim: 8
- 1m_vector_index_requested: auto
- 1m_vector_index: kdtree
- 1m_causal_root_hit_rate: 0
- 1m_causal_lattice_p95_ms: 0.3152
- 1m_metadata_service_p95_ms: 0.0076
- 1m_cross_layer_edges: 3
- full_ctest_completed: False
- ga_readiness_completed: False
- fuzz_completed: False
- soak_completed: False
- filesystem_gate_passed: False
- disk_pressure_gate_passed: False
- full_day_soak_profile_ready: False
- release_like_profile_ready: False
- graphenedb_use_faiss: False

## GA Host Attestation
- [PARTIAL] Host profile: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953\HOST_PROFILE.json`
- approved_host: False
- profile_label: release-candidate-smoke

## Latest Passing GA Benchmarks
- graphenedb_use_faiss: False
- vector_index_recall_requested: auto
- vector_index_recall: kdtree
- vector_index_recall_mean_recall_at_k: 1
- extraction_vector_index_requested: auto
- extraction_vector_index: auto
- storage_vector_index_requested: auto
- storage_vector_index: auto

## Latest GA Attempt
- [PASS] Latest attempted GA summary: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953\GA_READINESS_SUMMARY.md`
- [PARTIAL] Latest attempted GA host profile: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\ga-readiness\20260707-141953\HOST_PROFILE.json`
- profile_label: release-candidate-smoke
- approved_host: False

## Preview Hardware Profile
- [PASS] Latest preview hardware summary: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\PREVIEW_HARDWARE_SUMMARY.md`
- [PASS] Host profile: `C:\Users\vrast\Documents\Projects\test\New folder\graphenedb_v1\reports\preview-hardware\20260707-141656\HOST_PROFILE.json`
- intended_hardware: True
- profile_label: developer-preview
- graphenedb_use_faiss: False
- preview_vector_index_requested: auto
- preview_vector_index: kdtree
- preview_mean_recall_at_k: 1
- extraction_vector_index_requested: auto
- extraction_vector_index: flat
- storage_vector_index_requested: auto
- storage_vector_index: flat

## Pending Counts
- public_developer_preview: 0
- enterprise_ga: 10

## Pending For Public Developer Preview

## Pending For Enterprise GA
- Run a true 24-hour soak on a persistent machine.
- Run multi-hour coverage-guided fuzzing and preserve corpus/crash artifacts.
- Run the full default GA readiness harness on an approved build host without local policy exclusions.
- Run approved-host 100k/1M-node rich-workload profiles at target dimensions (`100k >= 384`, `1M >= 768`) with realistic metadata, causal density, extraction records, and lattice bonds, and preserve the release evidence.
- Run real disk-full, permission-denied, and rollback-failure tests on target filesystems.
- Integrate against the real KoshDB/LLM-Kosh runtime if it remains part of the release story.
- Complete the optional FAISS/HNSW vector index path with persistence, approved-host recall/latency evidence, and a final release-backend decision.
- Add release signing, semantic version tags, and final license review.
- Revisit encryption-at-rest and authentication/authorization if the product expands beyond the embedded-library security boundary.
- Rehearse operational recovery procedures on target release hosts and preserve evidence.

## Summary
GrapheneDB has strong local evidence for controlled-pilot and release-candidate readiness, but enterprise GA is still blocked on long-running soak/fuzz, target-host/full-filesystem campaigns, target-scale performance, live integration decisions, and release governance.
