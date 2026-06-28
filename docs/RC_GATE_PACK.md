# GrapheneDB v1 RC Gate Pack

This gate pack tests whether the v1 release candidate is credible as an embedded causal-memory DB core.

## Included gates

1. **Stress gate** — high-volume ingest, vector retrieval, causal retrieval, reopen/replay.
2. **Crash/recovery gate** — committed replay, uncommitted ignore, torn WAL tail recovery, invalid WAL failure, compaction gap safety.
3. **Fuzz/negative gate** — invalid vectors, NaN/Inf, bad edge endpoints, bad confidence, malformed WAL tails.
4. **Kosh adapter gate** — fake LLM-Kosh adapter proving metadata, memory ingest, causal bundle retrieval, persistence/reopen.
5. **Sanitizer gates** — ASAN/UBSAN and TSAN smoke profiles.
6. **FAISS option gate** — `GRAPHENEDB_USE_FAISS=ON` fails loudly when FAISS is unavailable.

## Commands

```bash
./scripts/run_rc_gate_pack.sh
./scripts/run_sanitizer_gates.sh
```

Optional full-scale knobs:

```bash
STRESS_INCIDENTS=33334 STRESS_QUERIES=200 STRESS_DIM=64 ./scripts/run_rc_gate_pack.sh
ASAN_STRESS_INCIDENTS=500 TSAN_STRESS_INCIDENTS=200 ./scripts/run_sanitizer_gates.sh
```

## Acceptance meaning

Passing this gate pack means **controlled pilot readiness** for an embedded causal-memory DB. It does not yet mean external enterprise GA.

Remaining non-v1.0-enterprise items include WAL rotation, coverage-guided fuzzing, real process-kill crash injection, production KoshDB integration, metadata indexes, stale lock recovery, and 1M+ soak testing.
