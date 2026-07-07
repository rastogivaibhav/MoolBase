# GrapheneDB v1 RC3 acceptance report

## Release name

`graphenedb_v1_rc3_productization_pack.zip`

## Current classification

**v1 RC3 / controlled pilot candidate**

This pack is suitable for another engineer to build, inspect, run, and challenge. It should not yet be represented as external enterprise GA.

## What RC3 adds over RC2

- GitHub-ready repository hygiene.
- One-command build/test/stress/fuzz/demo scripts.
- GitHub Actions CI workflow.
- CMake presets.
- Graphene uniqueness demo.
- API examples for coding memory, incident memory, and team brain.
- Better README.
- GA readiness scorecard.
- Claude/Codex handoff receipt.
- Acceptance report.

## Acceptance checklist

| Gate | Status | Evidence |
|---|---:|---|
| Clean C++ library structure | Pass | `include/graphene`, `src`, `tools`, `tests`, `bench`, `examples` |
| Release build | Pass | `scripts/build_release.sh` |
| Test suite | Pass in RC2 evidence, re-runnable in RC3 | `scripts/run_all_tests.sh`, `reports/RC_FINAL_CTEST_OUTPUT.txt` |
| Sanitizer smoke gates | Pass in RC2 evidence, re-runnable in RC3 | `scripts/run_sanitizers.sh`, `reports/RC_ASAN_UBSAN_OUTPUT.txt`, `reports/RC_TSAN_OUTPUT.txt` |
| 1M-node storage stress | Pass in RC2 evidence, re-runnable in RC3 | `scripts/run_1m_stress.sh`, `reports/RC_STRESS_1M_STORAGE_OUTPUT.txt` |
| Process-kill crash matrix | Pass in RC2 evidence, re-runnable in RC3 | `scripts/run_crash_matrix.sh`, `reports/RC_PROCESS_KILL_MATRIX_OUTPUT.txt` |
| Coverage-guided fuzz smoke | Pass in RC2 evidence, re-runnable in RC3 | `scripts/run_fuzz_smoke.sh`, `reports/RC_COVERAGE_FUZZ_OUTPUT.txt` |
| WAL rotation | Pass in RC2 evidence | `reports/RC_WAL_ROTATION_METADATA_LOCK_OUTPUT.txt` |
| Metadata search API | Pass in RC2 evidence and examples | `metadata_search`, `api_incident_memory.cpp` |
| Stale lock recovery | Pass in RC2 evidence | `reports/RC_WAL_ROTATION_METADATA_LOCK_OUTPUT.txt` |
| Kosh adapter gate | Pass at adapter/TSV level | `include/graphene/kosh_adapter.hpp`, `tests/test_rc_gate_real_kosh_adapter.cpp` |
| Long soak harness | Present and smoke-passed | `scripts/run_extended_ga_gates.sh`, `tests/test_rc_gate_soak.cpp` |
| Graphene uniqueness demo | Added in RC3 | `examples/graphene_uniqueness_demo.cpp` |

## What is proven

- The project now builds as a reusable embedded C++ library.
- The DB has basic durability through a framed, checksummed WAL and checkpoint/compaction path.
- Node content, vectors, metadata, and edges survive replay.
- Invalid dimensions and invalid edges are rejected by the current API.
- Causal retrieval returns a `MemoryBundle`, not only nearest-neighbour IDs.
- Signature-plane routing can reduce candidate space on structured data.
- The code has smoke coverage for crash recovery, fuzzing, sanitizers, process-kill recovery, metadata search, stale lock recovery, and 1M-node storage.

## What is not yet proven

- 24-hour soak has not been actually run in this chat environment.
- Multi-hour fuzzing has not been actually run in this chat environment.
- Live integration against the real upstream KoshDB/LLM-Kosh repository has not been run in this pack.
- Production-grade disk-pressure crash testing remains outstanding.
- Performance on 1M nodes with large real embeddings and rich causal graph density remains to be benchmarked.
- Persistent secondary metadata index files are not implemented.
- Vector search is still flat; approximate vector index integration remains future work.

## Acceptance recommendation

Use this as a **controlled pilot RC**.

Do not label it enterprise GA until the GA blockers in `GA_READINESS_SCORECARD.md` are resolved.
