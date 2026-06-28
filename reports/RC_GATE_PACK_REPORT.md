# GrapheneDB v1 RC Gate Pack Report

## Executive result

**Gate pack status: PASS for controlled pilot readiness.**

The RC now includes stress, crash/recovery, fuzz/negative, fake Kosh adapter, CLI smoke, sanitizer, and FAISS fail-loudly gates.

This does **not** mean external enterprise GA yet. It means the codebase has crossed from a retrieval spike into a credible embedded DB release candidate that can be hardened further.

## Code changes made in this gate pack

- Added RC crash/recovery tests.
- Added RC fuzz/negative tests.
- Added RC fake Kosh adapter integration test.
- Added RC stress test executable with configurable incident count, query count, and dimension.
- Added scripts to run the release and sanitizer gate packs.
- Fixed compaction/reload phantom-node risk by making resized placeholder node/edge slots invisible.
- Removed expensive per-write manifest rewrites; manifest is maintained on open/close/compact instead.
- Fixed CLI `put-node` optional flag handling.
- Added RC gate documentation and reports.

## Release ctest result

```text
100% tests passed, 0 tests failed out of 5
Total Test time (real) = 3.32 sec
```

Covered tests:

1. `graphenedb_tests`
2. `graphenedb_rc_crash_tests`
3. `graphenedb_rc_fuzz_tests`
4. `graphenedb_rc_kosh_adapter_tests`
5. `graphenedb_rc_stress_tests`

## 100k-node stress result

```text
incidents=33334
nodes=100002
edges=66668
dim=64
ingest_ms=16677.9
ingest_nodes_per_sec=5996.07
vector_p50_ms=13.5944
vector_p95_ms=14.5454
vector_p99_ms=14.9298
causal_p50_ms=2.44371
causal_p95_ms=2.6959
causal_p99_ms=2.84004
causal_hit_rate=1
avg_candidate_pct=6.25001
reopen_ms=2277.07
```

Observed storage footprint for this stress DB:

```text
~110 MB WAL
```

Interpretation:

- The Graphene causal lane is significantly faster than flat vector search on this structured causal-memory dataset.
- Signature-plane routing reduced candidates to ~6.25% of visible nodes.
- Reopen/replay at 100k nodes completed in ~2.3 seconds.
- WAL is still large because it is text-framed and not rotated/compacted automatically.

## Crash/recovery gate

Passed scenarios:

- Middle-ID delete + compaction + reopen does not resurrect phantom nodes.
- Uncommitted transaction after `BEGIN`/`PUT_NODE` is ignored.
- Directly written committed WAL transaction replays actual content/vector.
- Torn WAL tail after committed data is ignored safely.
- Valid unknown WAL op fails controlled instead of silently mutating state.

## Fuzz/negative gate

Passed scenarios:

- `dimension=0` open rejected.
- Wrong-dimension vector rejected.
- NaN vector rejected.
- Infinity vector rejected.
- Invalid edge endpoint rejected.
- Invalid confidence rejected.
- Edge to deleted endpoint rejected.
- Wrong-dimension query returns controlled empty/abstain result.
- 100 deterministic malformed WAL-tail cases produce either safe open or controlled error.

## Kosh adapter gate

Passed fake LLM-Kosh adapter scenario:

- Ingests Kosh-style memory objects into GrapheneDB.
- Preserves Kosh metadata through close/reopen.
- Retrieves causal memory bundle from symptom to architecture decision root.
- Returns `why_retrieved` evidence including signature-plane routing and causal path.

Limitation: this is a fake adapter test, not integration with the real KoshDB/LLM-Kosh repository.

## Sanitizer gates

ASAN/UBSAN:

```text
100% tests passed, 0 tests failed out of 4
ASAN/UBSAN smoke stress: PASS
```

TSAN:

```text
100% tests passed, 0 tests failed out of 4
TSAN smoke stress: PASS
```

Note: sanitizer stress profiles are intentionally smaller than the 100k release stress profile to keep runtime practical.

## CLI smoke gate

Passed:

- `init`
- `put-node`
- `put-edge`
- `inspect`
- `search`

## FAISS option gate

Passed. `GRAPHENEDB_USE_FAISS=ON` fails loudly when FAISS is unavailable:

```text
GRAPHENEDB_USE_FAISS=ON but FAISS headers/library were not found.
exit_code=1
```

## Current readiness classification

| Area | Status |
|---|---|
| Buildability | PASS |
| Unit/integration tests | PASS |
| Crash/recovery simulation | PASS |
| Fuzz-lite negative testing | PASS |
| 100k-node stress | PASS |
| Causal retrieval advantage on structured data | PASS |
| Sanitizers | PASS smoke profiles |
| CLI smoke | PASS |
| Fake Kosh adapter | PASS |
| Real KoshDB integration | NOT YET |
| 1M-node stress | NOT YET |
| Coverage-guided fuzzing | NOT YET |
| True process-kill crash matrix | NOT YET |
| WAL rotation/compaction policy | NOT YET |
| Metadata indexes | NOT YET |
| Stale lock recovery | NOT YET |

## Recommendation

Call this package:

**GrapheneDB v1 RC Gate Pack — controlled pilot ready.**

Do not yet call it:

**External enterprise GA** or **Qdrant/Neo4j/SQLite replacement**.

Next recommended milestone:

**v1.0 Pilot Release** with real KoshDB/LLM-Kosh adapter integration, WAL rotation, 1M stress profile, and process-kill crash injection.
