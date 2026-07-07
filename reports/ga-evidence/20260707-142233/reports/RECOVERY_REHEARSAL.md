# Recovery Rehearsal

GrapheneDB now has an operator recovery rehearsal that proves a backup can be used as a restored database, not only copied and validated during backup creation.

## Commands

Windows:

```powershell
.\scripts\run_recovery_rehearsal.ps1 -Cli .\build-release\graphenedb_cli.exe
```

POSIX:

```bash
CLI=./build-release/graphenedb_cli scripts/run_recovery_rehearsal.sh
```

## Gate Coverage

The rehearsal:

- imports extraction records with source-scoped IDs
- validates the source database
- compacts the source database
- creates a verified backup
- opens the backup as the restored database
- validates the restored database
- checks restored node and edge counts
- runs retrieval against the restored database
- checks restored lattice neighbors

## GA Use

This is the fast recovery rehearsal for developer-preview and release-candidate gates. Enterprise GA still needs the same rehearsal on target release hosts with preserved logs and representative data volumes.
