# Operational Recovery

GrapheneDB is an embedded database. Recovery is performed by opening the database directory with the same dimension and then running validation, compaction, or backup from the library API or `graphenedb_cli`.

## Stale Locks

`LOCK` protects a database directory from multiple writable processes.

Default behavior:

```bash
graphenedb_cli inspect /path/to/db 384 --json
```

If `LOCK` contains a dead process ID, open removes the stale lock and proceeds. If the process is alive, open fails with `LockBusy`.

Strict behavior:

```bash
graphenedb_cli inspect /path/to/db 384 --no-recover-stale-lock
```

Use strict mode in automation when an operator wants to detect any leftover lock file before deciding whether to recover.

## Suspected Corruption

Run:

```bash
graphenedb_cli validate /path/to/db 384 --json
```

Validation checks vector dimensions, edge endpoint integrity, visible lattice coordinates, duplicate lattice occupancy, lattice bond topology, and bond strength ranges.

If validation fails, preserve the whole database directory before attempting repair or re-import. The current v0.5 core does not include an automatic repair/rebuild tool.

## WAL Growth

For routine WAL cleanup:

```bash
graphenedb_cli compact /path/to/db 384 --json
graphenedb_cli inspect /path/to/db 384 --json
```

`compact` checkpoints visible records into `graphene.data` and truncates `graphene.wal`. `inspect` reports `wal_bytes` and `data_bytes`.

For ongoing retention, open with:

```bash
graphenedb_cli import-tsv /path/to/db 384 memories.tsv --wal-rotate-bytes 67108864
```

The same option applies to CLI commands that open the database.

## Backup Verification

Run:

```bash
graphenedb_cli backup /path/to/db 384 /path/to/backup --json
```

By default, backup verification opens the copied database and runs `validate()`. Use `--no-verify` only for emergency copies where preserving bytes is more important than immediate validation.

## Recovery Rehearsal

Run the rehearsal before release:

```bash
scripts/run_recovery_rehearsal.sh
```

or on Windows:

```powershell
.\scripts\run_recovery_rehearsal.ps1 -Cli .\build-release\graphenedb_cli.exe
```

The rehearsal imports extraction records, validates and compacts the source database, creates a verified backup, opens the backup as a restored database, validates it, runs retrieval, and checks restored lattice neighbors.

## Failed Checkpoints Or Backups

If `compact` or `backup` fails, keep the source directory unchanged and run:

```bash
graphenedb_cli validate /path/to/db 384 --json
graphenedb_cli inspect /path/to/db 384 --json
```

The deterministic fault-injection tests cover failed checkpoint writes, checkpoint renames, and backup copies. POSIX smoke tests cover permission-denied and file-size-limit disk pressure. Real target-host disk-full and permission-denied campaigns are still required before enterprise GA certification.
