# GrapheneDB Evidence Lab MVP — build report

Date: 2026-08-01

## Scope implemented

- FastAPI public gateway under `/v1/public`;
- anonymous expiring sessions and delete-now;
- inspectable/downloadable sample library;
- JSON, NDJSON/JSONL, canonical ZIP and multi-file CSV/TSV ingestion;
- canonical reference validation and public size limits;
- recorded-reference backend with explicit non-live labelling;
- subprocess live backend that launches a disposable `graphenedb_server` on loopback;
- atomic `/v1/extractions` ingest and `/v1/reason/runtime` invocation;
- evidence graph, metrics, receipt and event result contract;
- downloadable reproduction ZIP with SHA-256 inventory;
- responsive static frontend suitable as the ChatGPT Sites reference implementation;
- Docker Compose, local runner and GitHub workflow.

## Local validation completed

Environment:

- Linux x86-64;
- Python 3.13 runtime for local tests;
- FastAPI 0.128.2;
- Pydantic 2.13.4;
- GNU C++ 14.2.0 for the available GrapheneDB source snapshot.

Passed:

1. `pytest` — 4/4 gateway tests passed.
2. Python compilation — all gateway modules passed `py_compile`.
3. JavaScript syntax — `node --check apps/evidence_lab/site/app.js` passed.
4. Both sample `SHA256SUMS` inventories passed.
5. Sample loader produced deterministic dataset hashes.
6. Recorded-mode HTTP smoke passed:
   - health;
   - session creation;
   - sample listing;
   - sample run;
   - recorded/live boundary fields;
   - metrics, graph and compact receipt output.
7. The available GrapheneDB source snapshot configured and built `graphenedb_server` successfully.

## Live integration finding and remediation

The first live gateway run reached the real GrapheneDB server and failed during relation ingestion because the pilot extraction API resolves `external_id` references atomically within one extraction request. Sending nodes first and relations in separate source batches returned:

```text
relation endpoint external_id not found
```

The adapter was corrected to submit the complete canonical node-and-relation graph in one atomic `/v1/extractions` request. Per-edge public source, evidence-family and derivation lineage remains in relation metadata.

## Validation boundary

The corrected live adapter has not yet been rerun end-to-end against the exact current GitHub head in this report. Therefore:

- recorded-reference mode is validated locally;
- live server compilation is validated on the available source snapshot;
- the real API contract reached and exposed a genuine integration issue;
- the atomic-ingestion remediation is implemented;
- a final clean live run remains a release gate and is not represented as passed.

## Required next gates

1. Fresh checkout of this branch.
2. Build the exact-head `graphenedb_server`.
3. Run gateway in `EVIDENCE_LAB_BACKEND=live` mode.
4. Execute both sample packs and one uploaded canonical JSON dataset.
5. Verify `live=true` and `run_mode=live_graphenedb`.
6. Download and independently inspect each reproduction bundle.
7. Add malware scanning and production worker/container orchestration before public arbitrary upload.
8. Deploy the gateway behind TLS, WAF/rate limits and the ChatGPT Site.
