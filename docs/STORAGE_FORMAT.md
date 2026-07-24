# Storage Format

GrapheneDB stores an embedded database directory with these files:

- `MANIFEST`
- `graphene.data`
- `graphene.wal`
- `LOCK`

Current format versions:

| Component | Version | Meaning |
|---|---:|---|
| Manifest | 1 | Text manifest with dimension, logical version, IDs, txid, checksummed body, and component format fields. |
| Storage | 2 | Node/edge records include lattice coordinates, bond metadata, defect metadata, layer coupling, and bond strength. |
| WAL frame | 1 | Text frame: `payload_length|fnv1a_checksum|payload`. |
| Lattice | 1 | Axial hex coordinates `q,r,layer`; validated bond metadata. |
| Extraction | 1 | Source-scoped external IDs stored as node metadata; relation keys, evidence metadata, source URI, and extraction run IDs stored as edge/node metadata. |

`GrapheneDB::inspect()` reports these versions:

```text
manifest_format=1
storage_format=2
wal_frame_format=1
lattice_format=1
extraction_format=1
```

It also reports operational storage counters:

```text
wal_rotate_bytes=<configured threshold, or 0 when disabled>
wal_bytes=<current graphene.wal size>
data_bytes=<current graphene.data size>
```

`compact()` and byte-threshold rotation rewrite live records to `graphene.data`
and truncate `graphene.wal`. Checkpoint replacement flushes the temporary data
file, atomically replaces the canonical data file, flushes the containing
directory where supported, and then durably truncates the WAL. Derived lattice
sidecars are rebuilt from the canonical data/WAL state.

## Compatibility

Storage v2 reads the older v1 node and edge field counts used before lattice metadata existed. Missing lattice fields are treated as absent coordinates and default bond metadata.

The compatibility contract is:

- v1 data records can be opened by the current reader.
- v2 lattice records can be opened by the current reader.
- An invalid, unterminated final WAL fragment is treated as a torn tail and is
  replayed through the last valid frame. The fragment is truncated before the
  WAL is reopened for append.
- A malformed or checksum-invalid complete WAL frame is corruption and fails
  open.
- Uncommitted WAL transactions are ignored.
- Future incompatible format changes must bump `kStorageFormatVersion` and add fixture coverage before release.

## Manifest Validation

Open validates:

- manifest header
- checksum over the body before the `checksum=` line
- numeric fields
- required `dimension`
- required `next_txid`
- unsupported future component format versions

Corrupt manifests fail with `DataCorrupt`; unsupported future formats fail with `UnsupportedMode`.

## Non-Goals

The format is currently an embedded text/framed storage format, not a stable cross-language binary wire format. It is designed for local durability, testability, and compatibility gates first.
