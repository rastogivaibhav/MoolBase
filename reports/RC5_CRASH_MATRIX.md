# GrapheneDB RC5 Crash Matrix

## Current RC5 coverage

`graphenedb_rc5_crash_matrix_tests` exercises WAL-level lattice recovery cases:

- committed batch with v2 lattice node records and bond metadata replays
- committed extraction-shaped WAL records replay with source/external identity metadata
- replayed extraction records remain idempotent through `put_extraction()`
- uncommitted batch with no `COMMIT` is ignored
- torn WAL tail after a committed batch is ignored
- invalid committed lattice topology remains detectable through `validate()`
- leftover `graphene.data.tmp` and `MANIFEST.tmp` files from interrupted writes are ignored on reopen
- corrupt manifest checksum/header fail with `DataCorrupt`
- unsupported future manifest format fields fail with `UnsupportedMode`

This complements the existing RC crash gates for torn tails, unknown ops, compaction gaps, process-kill recovery on POSIX, WAL rotation, and stale-lock recovery.

`graphenedb_rc5_fault_injection_tests` adds deterministic I/O failure hooks:

- injected WAL append failure during `put_batch()` returns `IoError`
- failed batch append leaves no in-memory or durable partial state
- injected WAL write failure returns `IoError` before mutation state is applied
- injected WAL fsync failure returns `IoError`, rolls the append back to the prior WAL boundary, and does not replay the failed commit
- injected WAL append failure during `put_extraction()` returns `IoError`
- failed extraction append leaves no in-memory, metadata-index, or durable partial state
- injected checkpoint temp-write failure returns `IoError`
- failed checkpoint temp-write leaves live and reopened DB state valid
- injected checkpoint failure before rename returns `IoError`
- failed checkpoint leaves live and reopened DB state valid
- injected manifest rename failure during `close()` returns `IoError`
- manifest failure leaves WAL/data reopenable and valid
- injected backup copy failure returns `IoError`
- backup failure does not mutate the source DB

## Remaining GA crash work

The current RC5 matrix is still frame-level and deterministic. Enterprise GA needs fault injection or process-kill coverage across:

- before WAL append
- after `BEGIN`
- after each mutation record in a batch
- after `COMMIT`
- during checkpoint temp write
- before checkpoint rename (deterministic hook covered; process-kill still pending)
- after checkpoint rename before WAL truncate
- during manifest write (deterministic rename hook covered; process-kill still pending)
- during backup copy (deterministic copy hook covered; process-kill/disk-full still pending)
- disk-full and permission-denied cases
- real-device failure cases where rollback/truncate itself fails

Expected invariant after recovery: committed state survives, uncommitted state disappears, and `validate()` either returns OK or reports a precise controlled corruption reason.
