# GrapheneDB 0.6.0-rc1 Pilot Release Contract

## Scope

The pilot release consists of the embedded GrapheneDB C++ engine plus an optional single-node HTTP server. It is intended for controlled private pilots behind an approved TLS reverse proxy.

It is not a distributed database, multi-tenant control plane, SQL service, or internet edge server.

## API compatibility

- HTTP API major version: `v1`.
- Capability discovery: `GET /v1/version`.
- Every response includes `X-GrapheneDB-Version` and `X-GrapheneDB-API-Version`.
- Compatible endpoints are documented in `docs/api/openapi-v1.yaml`.
- Breaking HTTP changes require a new URL major version.

## Retry and write contract

`POST /v1/nodes` and `POST /v1/facts` support `Idempotency-Key`:

- keys are scoped to the endpoint;
- maximum key length is 128 characters;
- same key and same content returns the original node with HTTP 200;
- same key and different content returns HTTP 409;
- the key is stored in node metadata and survives restart/checkpoint.

`POST /v1/nodes/bulk` is one bounded `put_batch()` transaction. It returns `atomic=true`; a validation or admission failure inserts zero nodes.

`POST /v1/extractions` maps the versioned `put_extraction()` core contract:

- one request contains a bounded set of typed nodes and relations;
- the server generates vectors and assigns missing lattice coordinates;
- the core validates the complete request before one WAL transaction;
- `source_id` plus each `external_id` is the durable idempotency identity;
- an identical replay returns HTTP 200 with no new node or edge IDs;
- a changed node, relation, confidence, provenance, or evidence payload returns
  HTTP 409;
- a validation, endpoint-resolution, capacity, or commit failure leaves the
  request with zero partial writes.

The experimental `GDB-GL-0` learning surface follows the same durable
contract:

- `POST /v1/learning/episodes` stores one immutable episode through one
  `put_extraction()` transaction; tenant plus episode identity is replay-safe;
- identical episode and policy-decision retries return HTTP 200 without a new
  node, while changed retries return HTTP 409;
- policy evaluation and `POST /v1/reason/hypokosh` perform no durable writes;
- a current policy changes only through an approved append-only promotion or
  rollback event;
- evaluation-split and legal-hold episodes do not influence selection;
- this controlled-pilot credential does not provide cryptographic tenant
  isolation. An identity-aware gateway must bind principals to tenant IDs.

## HTTP framing

The compact server supports HTTP/1.0 and HTTP/1.1 requests with `Content-Length`. It deliberately rejects:

- JSON body endpoints without `Content-Length`;
- conflicting or malformed `Content-Length` headers;
- chunked transfer encoding;
- non-`application/json` bodies for JSON endpoints.

The service must remain behind Caddy, NGINX, Envoy, an ingress controller, or another approved edge proxy.

## Lifecycle

On SIGINT/SIGTERM the server:

1. stops accepting connections;
2. drains queued and active work;
3. checkpoints live state and rotates the WAL by default;
4. closes the database lock;
5. emits structured `shutdown_started`, `shutdown_checkpoint`, and `shutdown_complete` events.

Use `--no-shutdown-checkpoint` only when an external supervisor has already performed an explicit checkpoint and shutdown latency is more important than restart speed.

## Capacity

For one physical layer and radius `R`:

```text
capacity = 1 + 3R(R + 1)
```

Use `--expected-max-nodes` at startup. The server refuses a configuration whose radius cannot hold the declared target.

## Vector index selection

The server defaults to `--vector-index auto`. Auto uses KD-tree only for
low-dimensional vectors and the exact flat index for higher dimensions.
Operators may explicitly select `flat`, `kdtree`, or a compiled-in `faiss`
backend, but must preserve recall/latency evidence for the selected production
profile.
