# GrapheneDB Public Evidence Lab

This directory implements the public developer validation environment specified in `docs/PUBLIC_DEVELOPER_EVIDENCE_LAB_SPEC.md`.

## Included

- inspectable and downloadable CC0 sample packs;
- canonical JSON, NDJSON, ZIP and multi-file CSV/TSV loaders;
- unresolved-reference and public-size validation;
- archive traversal, symlink, nested-archive, compression-ratio and credential scanning;
- optional or mandatory ClamAV scanning;
- anonymous one-hour sessions and delete-now;
- recorded-reference backend for disconnected UI development;
- resource-limited local subprocess backend;
- Kubernetes one-job-per-run backend with no worker service token and deny-all networking;
- ingestion through `/v1/extractions` and reasoning through `/v1/reason/runtime`;
- run result, evidence graph, compact receipt and execution events;
- downloadable reproduction bundle with SHA-256 inventory;
- responsive frontend suitable as the ChatGPT Sites reference implementation;
- exact-head live validation and container build workflows.

## Claim boundary

`EVIDENCE_LAB_BACKEND=recorded` never runs GrapheneDB and is visibly labelled recorded output in every response and screen.

`EVIDENCE_LAB_BACKEND=live` requires `GRAPHENEDB_SERVER_BINARY` and launches a fresh resource-limited local GrapheneDB server and database directory for each run. This mode is suitable for local validation, not unrestricted public hosting.

`EVIDENCE_LAB_BACKEND=kubernetes` creates a restricted Kubernetes Job for every run. This is the required public deployment profile.

## Run locally in recorded-reference mode

```bash
cd apps/evidence_lab/gateway
python3 -m venv .venv
. .venv/bin/activate
pip install -e '.[all]'

EVIDENCE_LAB_BACKEND=recorded \
EVIDENCE_LAB_SAMPLES_DIR=../samples \
uvicorn evidence_lab.main:app --host 127.0.0.1 --port 8080
```

In another terminal:

```bash
python3 -m http.server 8088 -d apps/evidence_lab/site
```

Open `http://127.0.0.1:8088`.

## Run the exact-head live gate

From the repository root:

```bash
bash scripts/run_evidence_lab_live_gate.sh
```

The gate:

1. builds `graphenedb_server` from the current checkout;
2. starts the gateway in live mode;
3. executes two public samples;
4. uploads and executes a canonical JSON dataset;
5. verifies live receipts and exact commit identity;
6. downloads and checks every reproduction bundle;
7. deletes the anonymous session.

A valid run ends with:

```text
EVIDENCE_LAB_EXACT_HEAD_LIVE_GATE=PASS
```

## Public deployment

Use the Kubernetes profile in `deploy/evidence-lab/`.

The public gateway:

- requires ClamAV;
- rate limits by client and anonymous session;
- applies secure headers and request IDs;
- stores only private expiring session files;
- creates one isolated worker Job per run;
- never exposes `graphenedb_server` to the internet.

See `deploy/evidence-lab/README.md` for image builds, manifests, ChatGPT Sites configuration and the deployed live gate.

## Current boundaries

- structured data only;
- synchronous public requests while the isolated Job runs;
- private RWX storage is required by the supplied Kubernetes profile;
- document extraction is not included;
- historical `previous_stop_reference` remains a recorded benchmark policy because the current binary no longer implements that rule;
- this is a sandboxed developer alpha, not a hosted production database, semantic truth engine or enterprise-GA service.
