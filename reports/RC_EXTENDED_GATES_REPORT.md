# GrapheneDB v1 RC Extended Gate Report

## Scope covered

This package extends the previous RC gate pack with the eight requested gates:

1. 1M-node stress
2. real process-kill crash matrix
3. coverage-guided fuzzing
4. KoshDB / LLM-Kosh integration adapter
5. WAL rotation
6. metadata indexes
7. stale lock recovery
8. long soak testing harness

## Code changes made

- Added persistent WAL file descriptor for much faster append-heavy workloads.
- Added WAL rotation/checkpoint support through `DBOptions::wal_rotate_bytes`.
- Added stale LOCK recovery via `DBOptions::recover_stale_lock`.
- Added metadata index and public `metadata_search(key, value)` API.
- Added `KoshAdapter` with ingest/link/retrieve and a local LLM-Kosh/KoshDB TSV interchange harness.
- Added LLVM libFuzzer target for WAL-open/corruption-path fuzzing.
- Added process-kill recovery test using fork + SIGKILL.
- Added soak test that mixes writes, reads, deletes, compaction, reopen, and validate.
- Added 1M-node storage stress test.
- Fixed validation to ignore intentionally empty placeholder slots created by compacted ID gaps.

## Results from this environment

### Release CTest

`RC_FINAL_CTEST_OUTPUT.txt`

- 10/10 tests passed.

### 1M-node storage stress

`RC_STRESS_1M_STORAGE_OUTPUT.txt`

- 1,000,000 nodes
- dimension 2 storage profile
- ingest: ~20.9s
- vector p50: ~22.7 ms
- vector p95: ~60.7 ms
- reopen: ~4.7s
- max RSS: ~400 MB

This proves million-node embedded storage/reopen/vector-scan viability in a compact profile. It is not a full 768-dimensional embedding benchmark.

### Process-kill crash matrix

`RC_PROCESS_KILL_MATRIX_OUTPUT.txt`

- kill during normal WAL append workload: pass
- kill with WAL rotation enabled: pass
- kill during repeated compaction: pass
- stale lock recovery after killed process: pass

### Coverage-guided fuzzing

`RC_COVERAGE_FUZZ_OUTPUT.txt`

- LLVM libFuzzer WAL-open target built and ran 1,000 iterations in this environment.
- No crash found in the 1,000-run smoke fuzz pass.

This is a coverage-guided fuzz smoke gate, not a multi-hour fuzz campaign.

### KoshDB / LLM-Kosh adapter

`RC_REAL_KOSH_ADAPTER_OUTPUT.txt`

- Concrete `KoshAdapter` added.
- TSV interchange ingest tested.
- Causal link creation tested.
- Metadata preservation tested.
- Reopen and retrieval tested.

This validates a real local interchange adapter. It does not yet validate against a live external KoshDB/LLM-Kosh repository runtime because that external repo is not bundled in this package.

### WAL rotation + metadata index + stale lock

`RC_WAL_ROTATION_METADATA_LOCK_OUTPUT.txt`

- stale dead-PID lock recovery: pass
- live-PID lock protection: pass
- WAL rotation/checkpoint: pass
- metadata index visibility after delete/reopen: pass

### Soak

`RC_SOAK_10S_OUTPUT.txt`

- 10 second local soak pass from this environment.
- Mixed writes, searches, deletes, compactions, reopen, validate.

The harness supports longer runs through:

```bash
SOAK_SECONDS=3600 scripts/run_extended_ga_gates.sh
```

A true GA soak should be run for 6-24 hours outside this constrained session.

## Honest remaining limitations

- 1M full causal stress with edges and larger vectors is not yet the default pass profile; the package includes the earlier causal stress harness and a separate 1M storage stress gate.
- The external KoshDB/LLM-Kosh runtime was not present, so integration is validated via local interchange adapter rather than live repo linking.
- Coverage fuzzing was a smoke run, not a sustained CI fuzz campaign.
- Long soak was run as a 10s proof in this environment; script supports longer runs.
- Metadata indexes are in-memory rebuilt indexes, not persisted secondary index files.
- WAL rotation is checkpoint-based and simple; no multi-segment WAL retention policy yet.

## Current classification

GrapheneDB is now stronger than the previous RC gate pack and can reasonably be called:

**v1 RC2 / controlled pilot candidate**

It is still not an external enterprise GA database until longer fuzz/soak/live-adapter campaigns are run outside this session.
