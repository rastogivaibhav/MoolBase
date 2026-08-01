# GrapheneDB Public Evidence Lab MVP

This directory implements the first end-to-end vertical slice of the public developer validation environment specified in `docs/PUBLIC_DEVELOPER_EVIDENCE_LAB_SPEC.md`.

## Included

- two inspectable and downloadable CC0 sample packs;
- canonical JSON, NDJSON, ZIP and multi-file CSV/TSV loaders;
- reference validation and unresolved-reference rejection;
- anonymous one-hour sessions and delete-now;
- recorded-reference backend for disconnected UI development;
- live subprocess backend that launches a disposable `graphenedb_server` per run;
- ingestion through `/v1/extractions` and reasoning through `/v1/reason/runtime`;
- run result, evidence graph, compact receipt and execution events;
- downloadable reproduction bundle with SHA-256 inventory;
- responsive static frontend suitable as a ChatGPT Sites reference implementation;
- API tests and CI workflow.

## Claim boundary

`EVIDENCE_LAB_BACKEND=recorded` never runs GrapheneDB and is visibly labelled recorded output in every response and screen.

`EVIDENCE_LAB_BACKEND=live` requires `GRAPHENEDB_SERVER_BINARY` and launches a fresh local GrapheneDB server and database directory for each run. The gateway stops the process and deletes the workspace after the result is collected.

## Run locally in recorded-reference mode

```bash
cd apps/evidence_lab/gateway
python3 -m venv .venv
. .venv/bin/activate
pip install -e '.[test]'

EVIDENCE_LAB_BACKEND=recorded \
EVIDENCE_LAB_SAMPLES_DIR=../samples \
uvicorn evidence_lab.main:app --host 127.0.0.1 --port 8080
```

In another terminal:

```bash
python3 -m http.server 8088 -d apps/evidence_lab/site
```

Open `http://127.0.0.1:8088`.

## Run with the real GrapheneDB binary

Build the server first, then:

```bash
EVIDENCE_LAB_BACKEND=live \
GRAPHENEDB_SERVER_BINARY="$PWD/build/graphenedb_server" \
GRAPHENEDB_SOURCE_COMMIT="$(git rev-parse HEAD)" \
EVIDENCE_LAB_SAMPLES_DIR="$PWD/apps/evidence_lab/samples" \
uvicorn evidence_lab.main:app --host 127.0.0.1 --port 8080
```

The existing GrapheneDB server is not exposed publicly. The gateway spawns it on a random loopback port for a single run.

## Current MVP limits

- structured data only;
- synchronous runs;
- filesystem session store;
- one gateway instance;
- no malware scanner integration yet;
- no object-store presigned uploads yet;
- no container-per-run orchestrator yet;
- historical `previous_stop_reference` remains a recorded benchmark policy because the current binary does not implement the removed stopping rule.

These are explicit follow-on items, not hidden capabilities.
