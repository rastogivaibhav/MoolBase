# GrapheneDB RC5 ACID Report

## Scope

RC5 expands validation around the graphene-inspired lattice model and batch ingestion path.

## Current gates

- `graphenedb_lattice_tests`
  - required lattice coordinates
  - duplicate coordinate rejection
  - same-layer bond validation
  - defect bond behavior
  - lattice score in retrieval
  - lattice durability through compact/reopen
- `graphenedb_acid_lattice_tests`
  - old v1 record compatibility
  - checked-in old v1 storage fixture replay
  - checked-in v2 lattice/bond storage fixture replay
  - atomic batch rollback on invalid lattice edge
  - deterministic lattice placement to batch input
  - cross-layer bond consistency
  - snapshot isolation after delete
  - concurrent reader smoke
  - compact/backup/reopen durability
  - backup restore of extraction source/external ID metadata and lattice bonds
  - CLI validation and verified backup restore command coverage
- `graphenedb_rc5_crash_matrix_tests`
  - committed lattice batch WAL replay
  - missing `COMMIT` batch rollback
  - torn-tail recovery after a valid commit
  - invalid committed lattice topology reported by validation
  - corrupt manifest checksum/header detection
  - unsupported future manifest format rejection
- `graphenedb_rc5_fault_injection_tests`
  - injected WAL append/write failures
  - injected WAL fsync failure with rollback to the previous WAL boundary
  - injected extraction WAL failure
  - injected checkpoint temp-write and rename failures
  - injected manifest rename and backup copy failures

## Remaining GA hardening

- Process-kill crash injection at every WAL/checkpoint phase.
- Multi-hour fuzzing with corpus archive.
- Real disk-full and permission-denied testing on target filesystems.
- Real-device cases where a failed WAL write/fsync is followed by rollback/truncate failure.
- Batch transaction rollback under injected I/O failures.
- Binary fixture archive expansion for more historical versions.
