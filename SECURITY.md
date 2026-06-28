# Security policy

## Supported status

This repository is a v1 RC / controlled pilot candidate. It is not yet an enterprise GA security-certified product.

## Security-sensitive areas

- WAL parsing and replay.
- Framed file checksum validation.
- Lock-file handling.
- Path handling for backup/open/compact.
- Fuzz inputs under `fuzz/`.
- Metadata parsing in Kosh adapter TSV ingest.

## Reporting issues

For now, report security issues privately to the project owner before public disclosure.

## Required security checks before public release

- Multi-hour coverage-guided fuzzing.
- ASAN/UBSAN/TSAN clean runs.
- Disk-pressure crash tests.
- Path traversal review.
- Dependency/license review.
- Final public license selection.
