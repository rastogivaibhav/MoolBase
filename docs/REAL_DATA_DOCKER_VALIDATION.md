# Docker and Real-Data Dialectic Validation

## Scope

This validation answers two separate questions:

1. Does the POSIX server and complete GrapheneDB contract run in a Linux
   container?
2. Does dialectic retrieval preserve competing, source-backed classifications
   better than vector top-1 and the existing single-root causal retrieval on
   real incident records?

It does not establish autonomous root-cause discovery, PayPal-scale capacity,
or production efficacy.

## Reproducible Docker validation

The repeatable validation image is defined in
`deploy/Dockerfile.validation`.

```bash
docker build \
  -f deploy/Dockerfile.validation \
  -t graphenedb-validation:extraction-fix .
docker run --rm graphenedb-validation:extraction-fix
```

Validation performed on 2026-07-24:

- Linux/amd64 Debian Bookworm;
- GCC 12.2;
- validation image ID
  `sha256:46f0efa8e82df748fc501c888e8d79750c36b5bd2e20b94f9755c3a61c27e854`;
- 34 of 34 CTest contracts passed in 25.17 seconds;
- the suite included process-kill recovery, real filesystem failure tests,
  the 10,000-node storage smoke, dialectic and hyperedge contracts, server
  launch hardening, the pilot HTTP/lifecycle contract, OpenAPI parity, and
  container-security checks.

The hardened production image also built successfully:

```text
tag:  graphenedb-server:extraction-fix
id:   sha256:45d89a4eb546fec2c8cd22feb13928635964172dd49cdfc6472c19bd16b652dc
size: 28,594,132 bytes
```

A live isolated run was healthy as UID/GID `10001:10001`, used a read-only
root filesystem, admitted an authenticated request only with the required
forwarded-HTTPS header, and returned `atomic_extraction_ingest` and
`bounded_dialectic_reasoning` through `/v1/version`. A production-image smoke
request atomically inserted two typed nodes and one evidence edge, its replay
wrote nothing, and the resulting dialectic query made zero durable writes.
SIGTERM drained the server and exited with code zero.

## Real dataset

The evaluation uses the public
[`icco/postmortems`](https://github.com/icco/postmortems) corpus behind
[`postmortems.app`](https://postmortems.app/about). The project describes
itself as a public machine-readable corpus of post-incident reviews, extending
the `danluu/post-mortems` collection with categories and time metadata.

Pinned input:

```text
repository: https://github.com/icco/postmortems.git
commit:     0ed8afb0f8cfd83a34bbda270943ebdbf9661062
licence:    GPL-3.0
files:      242 Markdown records
usable:     176 records with UUID, source URL, title, and categories
```

The upstream corpus includes hand-curated fields and LLM-enriched fields.
This evaluation uses the supplied title, source URL, start time, and category
annotations. It does not treat the enriched prose as independent ground truth.

## Method

`graphenedb_real_postmortems_bench` creates:

- one root node for each annotated category;
- one symptom node for each usable incident;
- one observed causal edge from each annotated category to its incident;
- a source evidence reference containing the original postmortem URL;
- an `observed_at` value where the corpus supplies a valid RFC3339 start time.

The public incident title is used as the query vector. The deterministic
64-dimensional test embedding deliberately isolates database behavior from
the selection of an external production embedding model.

The three retrieval variants receive the same query:

- vector top-1 asks whether the nearest node is an annotated category root;
- existing causal retrieval asks whether its single selected root is one of
  the annotated categories;
- dialectic retrieval measures recall of every annotated category root and
  whether opposition preserves/challenges competing roots.

This is annotated-graph retrieval, not inference of a previously unknown
cause. Category edges are supplied from the corpus.

Run:

```bash
docker run --rm \
  -v /path/to/postmortems:/corpus:ro \
  graphenedb-validation:extraction-fix \
  /src/build-validation/graphenedb_real_postmortems_bench /corpus/data
```

The same corpus was then loaded entirely through the authenticated pilot API:

```bash
python3 scripts/server_real_postmortem_api_eval.py \
  ./build/graphenedb_server /path/to/postmortems/data
```

The harness sends one bounded `POST /v1/extractions` request, verifies
same-process replay, runs every dialectic query over HTTP, gracefully restarts
the server, verifies a second no-write replay, and runs storage and provenance
validation.

## Results

Linux-container result:

| Measure | Result |
|---|---:|
| Usable real incident records | 176 |
| Category roots | 7 |
| Annotated category links | 432 |
| Timestamped evidence-edge instances | 377 |
| Vector top-1 category hit rate | 0.000 |
| Existing causal any-category hit rate | 1.000 |
| Existing causal category micro-recall | 0.407 |
| Dialectic category micro-recall | 1.000 |
| Dialectic complete multi-root rate | 1.000 |
| Dialectic competing-root challenge rate | 1.000 |
| Dialectic provenance-safe record rate | 1.000 |
| Dialectic false-promotion rate | 0.000 |
| Vector p95 | 0.022 ms |
| Existing causal p95 | 0.069 ms |
| Dialectic p95 | 0.130 ms |

The result shows the intended behavioral distinction. Vector top-1 finds the
matching incident, not its linked explanatory categories. Existing causal
retrieval reaches one valid category but discards the other annotations,
yielding 40.7% link recall. Dialectic retrieval retains all competing roots,
ties every path to the original source URL, and explicitly challenges the
multi-root conclusion.

The latency numbers describe only 183 nodes and 432 edges in one in-memory
process. They are regression evidence, not capacity planning data.

API-level result:

| Measure | Result |
|---|---:|
| Atomic extraction status | HTTP 201 |
| Nodes committed in one transaction | 183 |
| Evidence edges committed in one transaction | 432 |
| Same-process replay | HTTP 200, zero writes |
| Post-restart replay | HTTP 200, zero writes |
| Dialectic category micro-recall | 1.000 |
| Dialectic complete multi-root rate | 1.000 |
| Dialectic provenance-safe record rate | 1.000 |
| Dialectic false-promotion rate | 0.000 |
| HTTP dialectic p95 | 2.293 ms |
| Storage/provenance validation | passed |

This closes the earlier API reachability gap: the complete typed causal
dataset can now be loaded through the public pilot contract without
per-node partial writes or duplicated server-side storage logic.

## Difference from other database classes

| Database class | Natural result for this evaluation | What GrapheneDB adds | What that class does better |
|---|---|---|---|
| Relational database | Rows plus joins/recursive CTEs | Integrated vector anchor, typed causal modes, provenance risk, bounded opposition, epistemic status | SQL, constraints, analytics, mature replication and operations |
| Vector database | Nearest incident records | Traversal from a semantic anchor to evidence-backed causal roots while preserving alternatives | Large-scale ANN, filtering, distributed indexing |
| Property graph database | All connected category/root nodes | Snapshot-pinned vector entry, origin/role policy, temporal/provenance checks, deterministic convergence/opposition | General graph query languages and mature distributed graph operations |
| GrapheneDB | Bounded evidence bundle and qualified synthesis | The combined behavior is in the embedded retrieval contract | It remains single-node and specialized |

None of these distinctions is exclusive in principle. Equivalent behavior can
be constructed on another database in application code. GrapheneDB's claim is
that the bounded causal/evidence behavior is a tested first-class retrieval
contract, not that other databases are incapable of implementing it.

## Findings that block a PayPal-scale claim

1. The pilot server's text embedding is a deterministic local hash, not a
   production semantic embedding service.
2. The corpus test validates retrieval of supplied annotations. A blinded test
   must start from raw alerts/logs/traces and score root-cause hypotheses
   against held-out postmortems.
3. GrapheneDB is not distributed. Hundreds of agents require tenant-aware
   admission, quotas, workload isolation, shard/replica architecture outside
   the embedded engine, backup/restore drills, and sustained concurrency/load
   evidence.
4. The real-data run is small. The 24-hour and 72-hour gates and larger
   incident/agent workload profiles remain mandatory.

The extraction blocker is resolved. The next product gates are a production
embedding boundary, a blinded raw-signal evaluation, tenant-aware external
admission/isolation for hundreds of agents, and the existing long-soak and
container-security evidence—not an unbounded model-world mutation API.
