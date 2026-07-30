# GrapheneDB Server HTTP Fix and Behaviour Report

## Change made

The minimal HTTP server previously performed a single `read()` call and treated whatever bytes arrived as the complete request. This worked with simple `curl` calls but failed with clients that split headers/body across TCP packets, such as Python `urllib`, causing empty node content.

The server now:

- Reads request data in a loop.
- Parses the header terminator `\r\n\r\n`.
- Parses `Content-Length` case-insensitively.
- Continues reading until the full body has arrived.
- Trims the body to the declared `Content-Length`.
- Supports bodies larger than the old 65 KB single-buffer limit.

## Targeted validation

Build target validation passed for:

- `graphenedb_server`
- `graphenedb_tests`
- `graphenedb_lattice_tests`
- `graphenedb_dense_hex_lattice_index_tests`
- `graphenedb_physical_lattice_storage_tests`
- `graphenedb_physical_lattice_primary_tests`

Runtime test results:

| Test | Result |
|---|---:|
| Health endpoint | passed |
| Python `urllib` insert | 300 / 300 inserted |
| Read node content after insert | passed |
| Lattice neighbour search | passed |
| Hybrid vector search | passed |
| Restart persistence | passed |
| Concurrent client inserts | 200 / 200 inserted |
| Large body insert/read | 70,027 chars preserved |

## Behaviour after fix

The server now behaves like a real HTTP write/read prototype instead of a curl-only demo.

Observed behaviour:

- 300 Python-client writes inserted correctly.
- Node content was preserved, not stored empty.
- Node `42` retained content before and after restart.
- Lattice search for node `42` returned 18 two-hop neighbours.
- Hybrid search returned relevant refund/escalation nodes.
- 200 concurrent client requests completed with 0 errors and 200 unique IDs.
- A 70 KB request body was stored and read back with the end marker preserved.
- Physical files were created and persisted: `graphene.lattice.bin`, `graphene.nodeidx`, `graphene.wal`, `graphene.lattice`.

## Remaining limitations

This is still a prototype DB server, not enterprise GA.

Remaining gaps:

1. It is still a minimal hand-written HTTP server rather than a hardened HTTP/gRPC framework.
2. It is single-process and effectively single-writer.
3. There is no auth, TLS, rate limiting, quota, or tenant isolation.
4. JSON parsing is still simple and should be replaced with a real parser.
5. Edge APIs are not complete enough for a full causal/evidence server.
6. Embedding is deterministic local preview hashing, not a production embedding model/provider.
7. Server-level 100k/1M API stress, crash recovery matrix, and long soak are still needed.

## Investment verdict

The fix materially improves correctness. The server is now worth one more hardening sprint.

Do not position it as a general database yet. Position it as a locality-aware AI memory server prototype with physical hex lattice storage.

The next sprint should focus on:

1. Real HTTP/gRPC layer.
2. Real JSON parser.
3. Full node + edge + evidence APIs.
4. 100k server write/read benchmark.
5. Crash/restart matrix against physical lattice files.
6. OpenAI-compatible and local ONNX embedding adapters.
