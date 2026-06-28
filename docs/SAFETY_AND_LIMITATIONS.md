# Safety and limitations

## Safety fixed in this v1 pass

- Content survives close/reopen.
- Vectors survive close/reopen.
- Metadata survives close/reopen.
- Invalid vector dimensions are rejected.
- Invalid edges are rejected.
- Torn WAL tail is ignored safely.
- Valid but uncommitted WAL transactions are ignored.
- Lock file blocks a second writable open.
- ASAN/UBSAN and TSAN tests pass.

## Still not full enterprise GA

This implementation is still an embedded v1 core, not a mature commercial DB.

Remaining hardening required before external GA:

- Larger 100k/1M/10M scale benchmarks.
- Randomised fuzz testing for WAL and query inputs.
- Crash injection during every write/checkpoint phase.
- Stronger manifest checksum verification.
- Stale lock recovery policy.
- WAL rotation policy.
- Metadata indexing.
- Production vector index option such as HNSW/FAISS with real integration tests.
- Encryption-at-rest option.
- More formal transaction batch API.
- Long-running soak tests.
