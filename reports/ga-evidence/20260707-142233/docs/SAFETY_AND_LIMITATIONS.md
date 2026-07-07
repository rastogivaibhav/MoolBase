# Safety and limitations

## Safety fixed in this v1 pass

- Content survives close/reopen.
- Vectors survive close/reopen.
- Metadata survives close/reopen.
- Invalid vector dimensions are rejected.
- Invalid edges are rejected.
- Invalid lattice coordinates, duplicate lattice occupancy, and non-neighbor lattice bonds are rejected when lattice validation is required.
- Torn WAL tail is ignored safely.
- Valid but uncommitted WAL transactions are ignored.
- Manifest body is checksummed and storage component versions are surfaced by `inspect()`.
- Corrupt manifest headers/checksums and unsupported future manifest formats fail loudly.
- WAL append write/fsync failures roll back to the previous valid boundary in the deterministic fault-injection path.
- WAL byte-threshold rotation and manual `compact` truncate retained WAL after checkpointing live records.
- Lock file blocks a second writable open.
- Dead-owner stale lock files are recovered on open by default, and CLI strict mode can fail instead.
- Batch insertion and extraction ingestion APIs are covered by persistence and idempotency tests.
- Backup verification opens the copied database and runs validation.
- Embedded-library security boundary is documented: no network listener, built-in auth service, or encryption-at-rest.
- Release packaging emits SHA-256 package and per-file checksum manifests.
- ASAN/UBSAN and TSAN tests pass.

## Still not full enterprise GA

This implementation is still an embedded v1 core, not a mature commercial DB.

Remaining hardening required before external GA:

- Larger 100k/1M/10M scale benchmarks on stable target hardware with preserved reports.
- Multi-hour randomized and coverage-guided fuzz testing for WAL, manifest, extraction, lattice, and query inputs.
- Process-kill crash testing across every write/checkpoint/backup phase.
- Real disk-full, permission-denied, and rollback-failure testing on target filesystems. POSIX smoke tests now cover permission denial and `RLIMIT_FSIZE` disk-pressure behavior, but enterprise GA still needs target-filesystem quota or volume-full campaigns.
- Full default GA readiness run on an approved build host without local Application Control exclusions.
- Metadata/external-ID indexes are currently in-process rebuildable indexes; define persistence/rebuild SLAs for the intended deployment.
- The optional FAISS/HNSW vector index is still rebuilt from durable node records on open; enterprise GA still needs index persistence, target-hardware recall/latency evidence, and a final backend decision.
- Encryption-at-rest option if required beyond filesystem/volume encryption.
- Authentication/authorization layer if required beyond embedded-library OS/process isolation.
- Release signing infrastructure.
- Long-running soak tests.
