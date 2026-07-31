# GrapheneDB Public Developer Evidence Lab

**Product and Technical Specification**  
Version: 0.1  
Status: Proposed for implementation  
Target release: Public developer alpha  
Primary experience: ChatGPT Sites  
Execution backend: Sandboxed external GrapheneDB service  

---

## 1. Executive summary

The GrapheneDB Public Developer Evidence Lab is a public, interactive validation environment for developers, research engineers and potential technical partners.

It must allow a visitor to:

1. inspect and download verified sample datasets;
2. upload a bounded dataset of their own;
3. validate and normalise that dataset before execution;
4. run the actual GrapheneDB binary in an isolated sandbox;
5. compare reasoning and retrieval policies;
6. inspect the evidence graph, governed outcome and compact epistemic receipt;
7. download the complete input, output and reproduction bundle;
8. reproduce the same run locally against an immutable GrapheneDB release.

The ChatGPT Site is the public interface. It must not host or expose the GrapheneDB process directly. A dedicated public gateway validates requests and dispatches one disposable GrapheneDB worker per run.

The product is not a public managed database service. It is a technical lab for reproducibility, adversarial testing and developer-partner evaluation.

---

## 2. Problem statement

A static demonstration cannot establish that GrapheneDB is executing real evidence control, lineage analysis, contradiction handling or frontier-aware recovery.

Developers evaluating the project need to be able to answer:

- What exact input entered the database?
- Which transformations were applied?
- Which evidence paths were selected or rejected?
- Which pieces of evidence share a source or derivation family?
- Why did the runtime answer, deepen, contest or abstain?
- How much search work did each policy perform?
- Can the result be independently reproduced?
- Does the system behave consistently on a dataset supplied by the evaluator?

The Evidence Lab addresses these questions by making the entire run inspectable and downloadable.

---

## 3. Product positioning

### 3.1 Product statement

> GrapheneDB Evidence Lab is a live, sandboxed environment for testing whether retrieved information forms sufficient, independent and contradiction-safe grounds for an agentic conclusion.

### 3.2 Core demonstration

The product demonstrates the transition:

```text
raw evidence
→ validated and normalised graph
→ GrapheneDB ingestion
→ bounded evidence retrieval
→ FiberBundle construction
→ admissibility and stability assessment
→ targeted recovery where needed
→ governed conclusion
→ compact epistemic receipt
```

### 3.3 What it is not

The Evidence Lab is not:

- an unrestricted public database cluster;
- a production decision authority;
- a semantic truth engine;
- an internet-edge deployment of the existing pilot server;
- a general-purpose document intelligence platform;
- a hosted replacement for Neo4j, Qdrant, Milvus or PostgreSQL;
- a long-term file-storage service;
- a public repository of user-uploaded data;
- an enterprise-GA service with an SLA.

---

## 4. Goals and non-goals

### 4.1 Goals

G1. A developer can run a verified sample within five minutes of opening the Site.

G2. A developer can upload structured data, inspect its normalised form and execute GrapheneDB without operator assistance.

G3. Every successful run displays the exact GrapheneDB version, commit, configuration and dataset hash.

G4. Every successful run produces a deterministic compact receipt and a downloadable reproduction bundle.

G5. Sample datasets and expected validation gates are public, inspectable and downloadable.

G6. Failures are visible and diagnosable; the frontend must not silently substitute mocked output.

G7. Anonymous public runs are isolated, resource bounded and deleted automatically.

G8. Serious developer partners can request expanded limits without changing the public security boundary.

G9. Aggregate validation metrics can be published without exposing user content.

G10. The public demo accurately preserves GrapheneDB's experimental-alpha claim boundary.

### 4.2 Non-goals for version 1

NG1. Arbitrary executable upload.

NG2. Public PDF and DOCX extraction without an editable review step.

NG3. Persistent multi-user workspaces.

NG4. Shared writable databases between visitors.

NG5. Unlimited graph size or unrestricted query depth.

NG6. Public access to GrapheneDB administrative endpoints.

NG7. Public access to governed-learning policy mutation endpoints.

NG8. Production-grade hosted multi-tenancy.

NG9. Automatic public publication of uploaded datasets or run pages.

NG10. Claims of semantic correctness based only on structural evaluation.

---

## 5. Target users

### 5.1 AI infrastructure engineer

Needs to evaluate whether GrapheneDB can sit after an existing retriever or vector database and govern evidence-backed agent decisions.

### 5.2 Database or storage engineer

Needs to inspect ingestion, durability, graph representation, deterministic outputs, package integration and runtime limits.

### 5.3 Agent-framework developer

Needs to test whether GrapheneDB can return answer, deepen, contest or abstain outcomes that an orchestration framework can act upon.

### 5.4 Research engineer

Needs transparent experiment inputs, policy variants, output metrics, checksums and exact reproduction instructions.

### 5.5 Technical founder or potential partner

Needs a fast technical proof, a clear product boundary and a path to obtain larger evaluation access.

### 5.6 Security and reliability reviewer

Needs explicit isolation, retention, upload validation, failure handling and no-cross-session-leakage evidence.

---

## 6. Experience principles

1. **Real before polished.** Every result displayed as live must originate from a real GrapheneDB execution.
2. **Inspect before execute.** Uploaded data is never run before validation and user confirmation.
3. **Normalisation is visible.** The user can inspect and download the exact graph submitted to GrapheneDB.
4. **Reproducibility is first class.** Every run produces input, configuration, output, checksums and scripts.
5. **Failure is evidence.** Crashes, timeouts, rejected schemas and non-resolution are displayed honestly.
6. **Private by default.** User uploads and run outputs are private to the session unless explicitly anonymised and published.
7. **Claim boundaries are persistent.** The experimental and non-semantic-truth limitations remain visible in the Lab and downloads.
8. **No evidence inflation.** Source, evidence-family and derivation lineage are visible in the UI.
9. **One system of record.** The Site, public API, ChatGPT App and local reproduction flow use the same public run contract.
10. **Backend version is never hidden.** Every run page identifies the binary version and source commit.

---

## 7. Information architecture

```text
/
├── Evidence Lab
│   ├── Use sample data
│   └── Upload your data
├── Dataset Inspector
├── Schema Mapper
├── Evidence Graph
├── Policy Comparison
├── Receipt Inspector
├── Download and Reproduce
├── Sample Library
├── Validation Dashboard
├── Why GrapheneDB
├── Developer Integration
├── Paper and Benchmarks
├── Known Limitations
├── Privacy and Security
└── Partner Access
```

---

## 8. Core user journeys

## 8.1 Journey A: run a verified sample

1. Visitor opens the Evidence Lab.
2. Visitor selects **Use sample data**.
3. Site lists sample packs with graph size, evidence families, challenge and expected gates.
4. Visitor opens the sample inspector.
5. Visitor views nodes, edges, evidence lineage, queries and manifest.
6. Visitor may download the original sample ZIP and checksums.
7. Visitor selects a query and a policy or chooses policy comparison.
8. Site creates a run.
9. Real execution stages stream to the page.
10. Site displays result, graph, policy metrics and receipt.
11. Visitor downloads the run bundle.
12. Visitor follows local reproduction instructions.

### Acceptance criteria

- No sign-in is required for a bounded sample run.
- The raw sample files are viewable before execution.
- The sample manifest identifies its licence and checksum.
- Live and recorded output are visually distinct.
- The result page includes the exact GrapheneDB commit and dataset hash.

## 8.2 Journey B: upload a structured dataset

1. Visitor selects **Upload your data**.
2. Site presents accepted formats, size limits, retention and prohibited-data notice.
3. Visitor accepts the upload terms.
4. Browser requests a short-lived upload URL.
5. File is uploaded directly to quarantine storage.
6. Gateway performs security and format preflight checks.
7. Validator parses the dataset without executing GrapheneDB.
8. Site displays schema errors and detected entities.
9. Visitor maps fields where required.
10. Normaliser produces the canonical GrapheneDB dataset.
11. Visitor inspects nodes, edges, evidence lineages and query definitions.
12. Visitor downloads the normalised dataset if desired.
13. Visitor explicitly confirms execution.
14. A disposable GrapheneDB worker runs the dataset.
15. Visitor inspects and downloads the result.
16. Session data is deleted automatically or via **Delete now**.

### Acceptance criteria

- Upload never triggers immediate database execution.
- Original and normalised file hashes are recorded.
- Unresolved node references block execution.
- Field mappings are shown and stored in the run bundle.
- User can cancel and delete the upload before execution.

## 8.3 Journey C: compare policies

1. Visitor selects a sample or uploaded dataset.
2. Visitor selects up to four allowed policies.
3. Gateway runs each policy against the same immutable dataset version and query.
4. Result page displays:
   - governed status;
   - selected target;
   - confidence;
   - independent evidence families;
   - contradiction mass;
   - cycles;
   - visited states;
   - edges examined;
   - deepest hop;
   - runtime duration;
   - receipt hash.
5. Visitor views a metric-difference explanation.
6. Visitor downloads all outputs in one comparison bundle.

### Acceptance criteria

- Dataset, query and binary version are identical across policies.
- Each policy executes separately; output is not derived by frontend transformation.
- Failed policy executions remain in the comparison with their failure state.

## 8.4 Journey D: mutate and challenge a sample

Allowed controlled mutations:

- duplicate an existing source;
- duplicate a derivation while keeping the same evidence family;
- add an independent corroborating source;
- add a material contradiction;
- remove a critical edge;
- add irrelevant high-confidence evidence;
- change temporal validity.

The Site shows a structured diff before rerunning.

### Acceptance criteria

- Original sample remains immutable.
- Mutation creates a new dataset version and hash.
- Result comparison explains which evidence-control metrics changed.

## 8.5 Journey E: publish an anonymised validation run

1. Visitor selects **Publish anonymised run**.
2. Site shows the exact fields proposed for publication.
3. Raw uploads, original filenames and free-text content are excluded by default.
4. Visitor explicitly consents.
5. Sanitiser verifies the public projection.
6. Published run receives a stable public URL.

Version 1 may defer this journey. Private-by-default run pages are mandatory.

## 8.6 Journey F: request developer partner access

Visitor provides only:

- name;
- work email;
- organisation;
- intended evaluation;
- area of interest;
- optional GitHub profile.

Partner approval is manual in version 1.

---

## 9. Supported data modes

## 9.1 Mode A: canonical GrapheneDB dataset ZIP

Preferred for exact database validation.

```text
graphenedb-dataset/
├── manifest.json
├── nodes.csv
├── edges.csv
├── evidence.csv              # optional when lineage is embedded in edges
├── queries.json
├── expected-gates.json       # sample packs only
├── README.md
└── SHA256SUMS
```

## 9.2 Mode B: separate CSV or TSV files

Required logical entities:

- nodes;
- edges;
- queries.

Evidence lineage may be embedded in nodes and edges or provided separately.

## 9.3 Mode C: JSON

One object containing manifest, nodes, edges and queries.

## 9.4 Mode D: NDJSON

Each record includes a `record_type` field:

- `node`;
- `edge`;
- `query`;
- `evidence`;
- `manifest`.

## 9.5 Mode E: retriever result set

Designed for developers evaluating GrapheneDB after an existing RAG or vector-retrieval system.

Minimum input:

```json
{
  "query": "Why did the service fail?",
  "retrieved_items": [
    {
      "external_id": "item-1",
      "text": "...",
      "source_id": "source-a",
      "retrieval_score": 0.91,
      "metadata": {}
    }
  ]
}
```

Because a flat retrieval set does not contain causal edges by definition, the Site must require one of:

- user-supplied relations;
- bounded rule-based mapping;
- proposed extraction followed by explicit user review.

## 9.6 Mode F: documents — later phase

Supported later:

- Markdown;
- plain text;
- PDF;
- DOCX.

Document flow must remain separate from structured GrapheneDB validation:

```text
document
→ extraction
→ proposed graph
→ user review and correction
→ confirmed graph
→ GrapheneDB execution
```

The UI must identify extraction-generated nodes and edges as proposed until confirmed.

---

## 10. Canonical public dataset contract

## 10.1 Manifest

```json
{
  "schema_version": 1,
  "dataset_id": "incident-missing-hop",
  "title": "Incident: hidden causal hop",
  "description": "Controlled multi-hop incident graph",
  "licence": "CC0-1.0",
  "created_at": "2026-08-01T00:00:00Z",
  "generator": "manual",
  "node_count": 14,
  "edge_count": 19,
  "query_count": 2,
  "files": [
    {"path": "nodes.csv", "sha256": "..."},
    {"path": "edges.csv", "sha256": "..."},
    {"path": "queries.json", "sha256": "..."}
  ]
}
```

## 10.2 Node contract

Required fields:

| Field | Type | Required | Notes |
|---|---|---:|---|
| external_id | string | yes | Unique within dataset version |
| content | string | yes | Maximum public-alpha length enforced |
| node_type | enum/string | yes | fact, concept, event, symptom, outcome, hypothesis, etc. |
| source_id | string | yes | Original source identity |
| evidence_family_id | string | recommended | Defaults conservatively to source_id |
| derivation_id | string | recommended | Identifies summaries or transformations |
| observed_at | RFC3339 | no | Temporal evidence timestamp |
| valid_from | RFC3339 | no | Temporal validity |
| valid_until | RFC3339 | no | Temporal validity |
| metadata | object | no | Bounded string metadata only |

## 10.3 Edge contract

| Field | Type | Required | Notes |
|---|---|---:|---|
| external_id | string | yes | Unique edge ID |
| from_external_id | string | yes | Must resolve |
| to_external_id | string | yes | Must resolve |
| relation | string | yes | causal, supports, contradicts, supersedes, etc. |
| role | enum | yes | support, opposition, noise or auto |
| confidence | number 0–1 | yes | Input confidence, not semantic truth |
| critical | boolean | no | Marks critical path edge |
| source_id | string | recommended | Relation source |
| evidence_family_id | string | recommended | Correlation lineage |
| derivation_id | string | recommended | Derivation lineage |
| observed_at | RFC3339 | no | Temporal evidence timestamp |
| metadata | object | no | Bounded strings |

## 10.4 Query contract

```json
{
  "query_id": "q1",
  "question": "Why did checkout failures increase?",
  "query_vector": [0.1, 0.2, 0.3],
  "query_signature": 123456,
  "target_external_id": "checkout-failure",
  "mode": "empirical",
  "options": {
    "max_cycles": 3,
    "enable_opposition_research": false
  }
}
```

For public samples, vectors and signatures may be supplied. For uploaded text-only datasets, the gateway may use the same documented server-side vector path as the current extraction endpoint, but the generated representation must be identified in the run configuration.

---

## 11. Sample library

Minimum release sample packs:

1. `incident-two-hop-baseline`
2. `incident-four-hop-hidden-chain`
3. `duplicate-source-inflation`
4. `independent-corroboration`
5. `material-contradiction`
6. `broad-retrieval-noise-trap`
7. `temporal-validity-mismatch`
8. `agent-memory-supersession`

Each pack must include:

- human-readable README;
- data files;
- manifest;
- expected structural gates;
- expected governed-status range, where deterministic;
- release version used to produce the reference output;
- checksums;
- licence;
- downloadable ZIP;
- one-click Site execution.

Reference output is not substituted for live output. It is used only for comparison and regression explanation.

---

## 12. System architecture

## 12.1 Logical architecture

```text
┌─────────────────────────────────────────────┐
│ ChatGPT Site                               │
│ UI, previews, graph, receipt, downloads    │
└──────────────────────┬──────────────────────┘
                       │ HTTPS
┌──────────────────────▼──────────────────────┐
│ Public Demo Gateway                        │
│ session, auth, quotas, orchestration, API  │
└───────┬───────────┬───────────┬────────────┘
        │           │           │
        │           │           └──────────────┐
        │           │                          │
┌───────▼──────┐ ┌──▼────────────────┐ ┌──────▼─────────┐
│ Object Store│ │ Metadata/Event DB │ │ Metrics/Logs   │
│ quarantine  │ │ sessions/runs     │ │ no raw content │
│ results     │ │ TTL/state         │ │                │
└───────┬──────┘ └────────┬──────────┘ └────────────────┘
        │                 │
┌───────▼─────────────────▼───────────────────┐
│ Job Queue / Orchestrator                    │
└──────────────────────┬──────────────────────┘
                       │ one job per run
┌──────────────────────▼──────────────────────┐
│ Disposable GrapheneDB Worker               │
│ validate final input → ingest → run → pack │
│ no inbound internet, restricted egress     │
└─────────────────────────────────────────────┘
```

## 12.2 Reference deployment

The implementation may be cloud neutral. A reference GCP deployment is:

- ChatGPT Sites: public frontend;
- Cloud Run service: public demo gateway;
- Cloud Storage: quarantine, normalised datasets and result bundles;
- Pub/Sub: job queue;
- Cloud Run Jobs: one isolated worker per execution;
- Firestore or PostgreSQL: session, dataset and run metadata;
- Secret Manager: gateway-to-worker and service credentials;
- Artifact Registry: immutable worker images;
- Cloud Armor or equivalent: WAF and rate controls;
- Cloud Logging and Monitoring: operational telemetry.

Equivalent AWS, Azure, Fly.io or Render deployments are acceptable if all isolation and lifecycle requirements are preserved.

## 12.3 ChatGPT Sites boundary

The Site owns:

- pages and navigation;
- input forms;
- local presentation state;
- public sample discovery;
- field mapping UI;
- graph visualisation;
- SSE or polling client;
- receipt and bundle presentation;
- partner access form;
- privacy, limitations and disclosure pages.

The Site must not contain:

- a permanent gateway secret;
- a direct GrapheneDB API key;
- database filesystem access;
- long-lived private upload URLs;
- admin endpoints;
- hidden reference results presented as live.

## 12.4 Public gateway responsibilities

The gateway owns:

- anonymous session issuance;
- optional partner authentication;
- presigned upload creation;
- upload metadata and consent record;
- rate limits and quotas;
- dataset state machine;
- validation orchestration;
- normalisation orchestration;
- run creation;
- worker dispatch;
- event streaming;
- signed artifact downloads;
- deletion requests;
- aggregate metrics;
- feedback intake.

## 12.5 Worker responsibilities

A worker must:

1. start from an immutable image containing one GrapheneDB release;
2. verify the dataset and configuration hashes;
3. create a new disposable working directory;
4. ingest only the normalised dataset;
5. execute the selected query and policy;
6. emit structured stage events;
7. produce graph, trace, receipt and timings;
8. produce the reproduction bundle;
9. write artifacts to the result store;
10. terminate and destroy its workspace.

---

## 13. Reuse of the existing GrapheneDB API

The current pilot API already provides useful internal contracts including:

- `/v1/health` and `/v1/ready`;
- `/v1/version`;
- `/v1/extractions` for bounded typed causal ingestion;
- `/v1/search/hybrid`;
- `/v1/retrieve/bundle` and `/v1/retrieve/explain`;
- `/v1/reason/dialectic`;
- `/v1/reason/hypokosh`;
- `/v1/metrics` and `/v1/metrics/prometheus`.

The public Site must not expose these endpoints directly.

The public gateway should either:

- invoke the GrapheneDB embedded C++ API inside the worker; or
- invoke the existing server only over a private loopback or worker-local network.

The following current endpoint groups must not be publicly routable:

- `/v1/admin/*`;
- governed-learning write endpoints;
- raw node and edge mutation endpoints outside the bounded import flow;
- filesystem backup destinations;
- capacity or internal storage inspection that exposes host details.

---

## 14. Public API contract

Base path:

```text
/v1/public
```

## 14.1 Sessions

### `POST /sessions`

Creates an anonymous or partner session.

Response:

```json
{
  "session_id": "ses_01...",
  "access_tier": "anonymous",
  "expires_at": "2026-08-01T01:00:00Z",
  "limits": {
    "upload_bytes": 5242880,
    "max_nodes": 5000,
    "max_edges": 20000,
    "max_queries": 5,
    "max_cycles": 3
  }
}
```

### `DELETE /sessions/{session_id}`

Deletes all session-owned datasets, uploads, runs and artifacts as soon as practical.

## 14.2 Samples

- `GET /samples`
- `GET /samples/{sample_id}`
- `GET /samples/{sample_id}/files`
- `GET /samples/{sample_id}/download`
- `POST /samples/{sample_id}/fork`

## 14.3 Uploads

### `POST /uploads`

Returns a short-lived presigned URL after validating declared metadata and terms acceptance.

### `POST /uploads/{upload_id}/complete`

Confirms upload completion and schedules preflight validation.

### `GET /uploads/{upload_id}`

Returns upload and preflight status.

## 14.4 Datasets

- `POST /datasets/from-upload`
- `GET /datasets/{dataset_id}`
- `GET /datasets/{dataset_id}/preview`
- `POST /datasets/{dataset_id}/mapping`
- `POST /datasets/{dataset_id}/normalise`
- `GET /datasets/{dataset_id}/validation`
- `GET /datasets/{dataset_id}/download`
- `POST /datasets/{dataset_id}/versions`
- `DELETE /datasets/{dataset_id}`

## 14.5 Runs

### `POST /runs`

```json
{
  "dataset_version_id": "dsv_01...",
  "query_id": "q1",
  "policy": "targeted_frontier_aware",
  "publish": false
}
```

### `POST /runs/compare`

```json
{
  "dataset_version_id": "dsv_01...",
  "query_id": "q1",
  "policies": [
    "one_pass",
    "previous_unchanged_bundle_stop",
    "targeted_frontier_aware",
    "broad_forced_expansion"
  ]
}
```

### Read endpoints

- `GET /runs/{run_id}`
- `GET /runs/{run_id}/events`
- `GET /runs/{run_id}/graph`
- `GET /runs/{run_id}/trace`
- `GET /runs/{run_id}/receipt`
- `GET /runs/{run_id}/bundle`
- `DELETE /runs/{run_id}`

Events may use server-sent events. Polling fallback is required.

## 14.6 Feedback and partner access

- `POST /feedback`
- `POST /partner-access-requests`

Feedback payloads must not include raw datasets by default.

---

## 15. State models

## 15.1 Dataset state

```text
created
→ uploaded
→ quarantined
→ preflight_passed | rejected
→ parsed
→ mapping_required | mapped
→ normalised
→ validation_passed | validation_failed
→ executable
→ expired | deleted
```

## 15.2 Run state

```text
created
→ queued
→ provisioning
→ ingesting
→ retrieving
→ reasoning
→ packaging
→ completed
```

Terminal failure states:

- rejected;
- timeout;
- resource_exceeded;
- ingestion_failed;
- execution_failed;
- packaging_failed;
- cancelled;
- expired;
- deleted.

A run must never move from a failure state to completed without a new run ID.

---

## 16. Real execution event contract

Example event stream:

```json
{"sequence":1,"stage":"accepted","message":"Run accepted"}
{"sequence":2,"stage":"worker_ready","message":"GrapheneDB worker started"}
{"sequence":3,"stage":"input_verified","dataset_sha256":"..."}
{"sequence":4,"stage":"ingestion_complete","nodes":427,"edges":692}
{"sequence":5,"stage":"bundle_complete","paths":14,"evidence_families":7}
{"sequence":6,"stage":"recovery_started","reason":"missing critical hop"}
{"sequence":7,"stage":"recovery_complete","cycle":1,"visited_states":13}
{"sequence":8,"stage":"governed_result","status":"provisionally_resolved"}
{"sequence":9,"stage":"receipt_complete","content_hash":"..."}
{"sequence":10,"stage":"bundle_ready","download_available":true}
```

No simulated percentages are permitted.

---

## 17. Result contract

A completed run returns:

```json
{
  "run_id": "run_01...",
  "execution": {
    "graphenedb_version": "v0.6.0-alpha.1",
    "commit": "...",
    "worker_image_digest": "sha256:...",
    "started_at": "...",
    "completed_at": "...",
    "duration_ms": 438
  },
  "input": {
    "dataset_version_id": "dsv_01...",
    "dataset_sha256": "...",
    "query_id": "q1",
    "policy": "targeted_frontier_aware"
  },
  "metrics": {
    "nodes_ingested": 427,
    "edges_ingested": 692,
    "cycles": 2,
    "visited_states": 16,
    "edges_examined": 41,
    "deepest_hop": 4,
    "independent_evidence_families": 2,
    "contradiction_mass": 0.14,
    "final_energy": 0.09
  },
  "outcome": {
    "status": "provisionally_resolved",
    "primary_node": 42,
    "confidence": 0.88,
    "residual_uncertainty": []
  },
  "receipt": {
    "content_hash": "...",
    "bundle_hash": "..."
  },
  "artifacts": {
    "graph": true,
    "trace": true,
    "reproduction_bundle": true
  }
}
```

---

## 18. Reproduction bundle

Every successful run generates:

```text
graphenedb-run-<run_id>/
├── source/
│   ├── original-upload.<ext>        # excluded from shared anonymised bundles
│   └── original-upload.sha256
├── normalised/
│   ├── manifest.json
│   ├── nodes.csv
│   ├── edges.csv
│   ├── queries.json
│   └── SHA256SUMS
├── execution/
│   ├── configuration.json
│   ├── environment.json
│   ├── events.ndjson
│   ├── trace.json
│   ├── policy-comparison.json
│   └── timings.json
├── result/
│   ├── compact-receipt.json
│   ├── evidence-graph.json
│   ├── residual-uncertainty.json
│   └── summary.md
├── reproduce.sh
├── reproduce.ps1
├── README.md
└── SHA256SUMS
```

Requirements:

- all artifact hashes are included;
- scripts reference an immutable tag and verified commit;
- scripts fail when the local version differs;
- environment-dependent timings are not treated as deterministic;
- core structural output and receipt-hash expectations are documented;
- raw user data is included only in the private download bundle.

---

## 19. Policy definitions

The public comparison must use frozen, documented policies.

### P1. One pass

- one bounded evidence construction pass;
- no recursive recovery.

### P2. Previous unchanged-bundle stop

- historical stopping behaviour retained only for comparison;
- stops when the completed bundle remains unchanged.

### P3. Targeted frontier-aware recovery

- current remediation policy;
- allows bounded recovery while traversal frontier progresses;
- follows defect-specific escape tasks;
- maximum three cycles in public alpha.

### P4. Broad forced expansion

- comparison baseline;
- forces broad expansion up to the same cycle budget;
- not the default production policy.

Policy IDs and exact options must be versioned in the backend and included in every result.

---

## 20. Frontend specification

## 20.1 Landing page

Must communicate within one screen:

- the evidence-sufficiency problem;
- that this is a live technical lab;
- two equal entry points: sample or upload;
- experimental-alpha boundary;
- paper, GitHub and reproduction links.

## 20.2 Sample library

Each card shows:

- title;
- failure mode;
- nodes and edges;
- evidence-family count;
- contradiction count;
- expected challenge;
- files and licence;
- actions: preview, download, run, compare.

## 20.3 Dataset inspector

Tabs:

- Overview;
- Files;
- Nodes;
- Edges;
- Evidence lineage;
- Queries;
- Validation;
- Raw JSON.

Supports pagination and client-side filtering without loading the entire large partner dataset into the browser.

## 20.4 Schema mapper

Capabilities:

- map uploaded columns to canonical fields;
- preview first 100 records;
- show type errors;
- show unresolved references;
- choose conservative lineage defaults;
- save mapping as part of dataset version;
- download mapping JSON.

## 20.5 Evidence graph

Visual semantics:

- green: independent support;
- red: opposition or material contradiction;
- grey: quarantined noise;
- amber dotted: incomplete frontier;
- thick path: selected support path;
- linked outline or grouping: same evidence family;
- superseded nodes visually retained, not deleted.

Node/edge inspector shows:

- source ID;
- evidence-family ID;
- derivation ID;
- relation role;
- confidence;
- temporal validity;
- selected/discarded state;
- verifier status;
- path membership.

## 20.6 Run console

Displays real stage events and resource use.

Required banners:

```text
LIVE GRAPHENEDB EXECUTION
```

or

```text
RECORDED REFERENCE OUTPUT — LIVE BACKEND NOT USED
```

## 20.7 Result page

Panels:

- governed status;
- conclusion and uncertainty;
- evidence metrics;
- graph;
- selected/discarded paths;
- policy metrics;
- compact receipt;
- version and checksums;
- download and reproduce.

## 20.8 Accessibility

- keyboard-operable navigation;
- graph has a tabular alternative;
- status is not encoded only by colour;
- WCAG 2.1 AA contrast target;
- focus management for modal and upload flows;
- screen-reader labels for metrics and errors.

---

## 21. Upload security

## 21.1 Allowlist

Public version 1:

- `.csv`;
- `.tsv`;
- `.json`;
- `.ndjson`;
- `.zip` containing only allowed formats and manifest files.

Rejected:

- executables;
- scripts outside generated reproduction downloads;
- shared libraries;
- disk images;
- office macros;
- nested archives;
- encrypted archives;
- symbolic links;
- device files.

## 21.2 Preflight pipeline

```text
extension allowlist
→ MIME and magic-byte validation
→ declared-size validation
→ archive-entry inspection
→ decompressed-size and compression-ratio limit
→ path traversal and symlink rejection
→ malware scan
→ secret/credential scan
→ UTF-8 and parser safety checks
→ schema validation
→ quarantine decision
```

## 21.3 Worker isolation

Worker requirements:

- one run per container or microVM;
- non-root user;
- read-only root filesystem;
- writable temporary directory only;
- no host filesystem mount;
- no Docker socket;
- no cloud metadata endpoint access;
- no inbound public network;
- no general internet egress;
- CPU, memory, process and wall-clock limits;
- seccomp/AppArmor/gVisor or equivalent where available;
- automatic termination after completion or timeout;
- immutable image digest recorded in result.

## 21.4 API security

- TLS only;
- CORS allowlist restricted to published Site origins;
- no permanent credential in browser code;
- short-lived session token;
- short-lived signed upload and download URLs;
- WAF and IP-based abuse controls;
- endpoint-specific rate limits;
- request body and header limits;
- idempotency keys for run creation;
- replay rejection for changed uploads;
- structured audit events without raw input content.

---

## 22. Privacy and retention

## 22.1 Public anonymous tier

- raw upload retention: maximum 60 minutes after last activity;
- normalised dataset retention: maximum 60 minutes;
- result artifact retention: maximum 60 minutes;
- operational metadata: up to 30 days, without raw graph content;
- immediate deletion action available;
- no use of uploaded data for model training or product examples without separate explicit permission;
- no public sharing by default.

## 22.2 Partner tier

- default retention: 24 hours;
- configurable only after explicit partner agreement;
- separate access control;
- larger limits;
- same worker isolation;
- private results by default.

## 22.3 Prohibited data notice

Before upload, users must acknowledge:

- do not upload confidential or proprietary data without authority;
- do not upload personal data unless authorised and necessary;
- do not upload medical records or protected health information;
- do not upload payment-card or financial-account data;
- do not upload API keys, credentials or secrets;
- do not upload third-party content without processing rights.

## 22.4 Logging rules

Never log:

- node content;
- source document text;
- original filenames where avoidable;
- free-text queries in aggregate telemetry;
- evidence URIs containing secrets;
- raw receipt uncertainty strings in public analytics.

Log only identifiers, counts, durations, statuses, error codes and hashes needed for operation.

---

## 23. Limits and quotas

Initial proposed limits:

| Limit | Anonymous | Approved partner |
|---|---:|---:|
| Upload size | 5 MB | 50 MB |
| Decompressed archive | 20 MB | 200 MB |
| Nodes | 5,000 | 50,000 |
| Edges | 20,000 | 200,000 |
| Queries per dataset | 5 | 50 |
| Policies per comparison | 4 | 4 |
| Max cycles | 3 | Configurable up to approved cap |
| Concurrent runs | 1 | 3 |
| Run timeout | 120 seconds | 600 seconds |
| Session retention | 60 minutes | 24 hours |

These are product limits, not GrapheneDB capacity claims. They must be tuned from measured resource use.

---

## 24. Observability

## 24.1 Operational metrics

- upload acceptance/rejection count;
- validation duration;
- normalisation duration;
- queue delay;
- worker provisioning duration;
- ingestion duration;
- reasoning duration;
- packaging duration;
- p50, p95 and p99 total run duration;
- worker CPU and memory peak;
- timeouts;
- crashes;
- result status distribution;
- deletion-lag SLO;
- rate-limit events;
- malware and secret-scan rejection counts.

## 24.2 Public aggregate dashboard

May display:

- total sample runs;
- total user-upload runs;
- successful runs;
- failures and timeouts;
- governed-status distribution;
- latency percentiles;
- mean visited states by policy;
- sample-pack reproduction rate;
- GrapheneDB release currently served.

Must not display:

- user text;
- filenames;
- dataset names supplied by users;
- source IDs;
- graph content;
- individual IP or identity;
- partner organisation metrics without consent.

## 24.3 Correlation identifiers

Use:

- session ID;
- upload ID;
- dataset ID;
- dataset version ID;
- run ID;
- worker job ID;
- artifact bundle ID.

Never reuse a user-supplied identifier as a trusted internal identifier.

---

## 25. Reliability requirements

### SLO targets for public alpha

- sample-run API availability: 99% monthly, excluding announced maintenance;
- sample run p95 completion: under 15 seconds for published bounded samples;
- structured upload validation p95: under 10 seconds at public limits;
- deletion request accepted: under 2 seconds;
- session data removed: within 15 minutes of explicit delete and within retention policy after expiry;
- no cross-session data exposure;
- reproducibility bundle generated for at least 99% of completed runs.

These are launch targets and must be validated before publication.

### Failure behaviour

- run timeout returns `timeout`, never a generic success;
- worker crash preserves diagnostic metadata but not raw public logs;
- failed artifact packaging does not hide a successful database result;
- repeated idempotent request returns the existing run ID;
- changed replay creates a new dataset version and run;
- partial uploads are expired and removed.

---

## 26. Testing strategy

## 26.1 Unit tests

- schema parsers;
- CSV/TSV/JSON/NDJSON adapters;
- mapping rules;
- hash and manifest generation;
- policy configuration;
- quota enforcement;
- retention calculations;
- public projection sanitisation.

## 26.2 Integration tests

- signed upload flow;
- quarantine to validation transition;
- normalisation;
- worker dispatch;
- GrapheneDB ingestion;
- real reasoning execution;
- event streaming;
- receipt retrieval;
- bundle generation;
- explicit deletion.

## 26.3 Security tests

- ZIP Slip and path traversal;
- symlink archive entries;
- zip bomb and decompression ratio;
- malformed CSV and JSON;
- parser depth and record-count exhaustion;
- large field values;
- MIME spoofing;
- credential patterns;
- unauthorised artifact access;
- CORS bypass;
- session fixation;
- ID enumeration;
- worker egress and metadata endpoint blocking;
- cross-session storage access;
- deletion verification.

## 26.4 End-to-end tests

Playwright journeys:

1. sample preview → run → receipt → download;
2. sample policy comparison;
3. CSV upload → mapping → validation → run;
4. invalid edge reference rejection;
5. prohibited archive rejection;
6. timeout visibility;
7. delete-now flow;
8. private run inaccessible from a separate browser context;
9. recorded-output banner when backend disabled;
10. mobile and keyboard-accessible paths.

## 26.5 Reproducibility tests

For every sample:

- hosted run;
- bundle download;
- clean local clone at tagged version;
- reproduction script execution;
- dataset hashes match;
- structural metrics match;
- deterministic receipt content hash matches where intended;
- environment-sensitive timings are ignored.

---

## 27. Release gates

Public launch is blocked until all P0 gates pass.

### P0 legal and public-release gates

- [ ] Placeholder code licence replaced with approved public licence.
- [ ] Employer/IP and confidentiality clearance obtained.
- [ ] Sample datasets have redistribution rights and licences.
- [ ] Privacy policy published and linked on every page.
- [ ] Terms and prohibited-data notice published.
- [ ] Security reporting route published.

### P0 GrapheneDB gates

- [ ] Immutable release tag created.
- [ ] Exact tagged alpha gate passes from a fresh clone.
- [ ] Full test suite passes on the release platform.
- [ ] Package consumer test passes.
- [ ] Sanitizer baseline passes.
- [ ] Controlled intervention benchmark reproduces.
- [ ] Sample-pack outputs frozen and checksummed.

### P0 Evidence Lab gates

- [ ] Public sample flow executes the real binary.
- [ ] Structured upload flow passes end to end.
- [ ] Worker isolation verified.
- [ ] No permanent browser credential.
- [ ] Delete-now verified.
- [ ] Cross-session access tests pass.
- [ ] Reproduction bundle executes locally.
- [ ] Live versus recorded mode is unmistakable.
- [ ] Abuse and upload-security tests pass.
- [ ] Public Site tested from an external logged-out browser.

### P1 post-launch gates

- partner tier;
- document extraction with graph review;
- anonymised public run publishing;
- ChatGPT App/MCP integration;
- expanded sample packs;
- external benchmark submission workflow.

---

## 28. Delivery plan

## Phase 0: release and contract foundation

Deliverables:

- frozen GrapheneDB public tag;
- public licence decision;
- canonical dataset schema;
- public API OpenAPI file;
- sample-pack format;
- worker image;
- threat model.

Estimated effort: 3–5 engineering days plus legal/IP decisions.

## Phase 1: sample-first live lab

Deliverables:

- ChatGPT Site shell;
- sample library;
- sample file inspector and download;
- public gateway session flow;
- live sample execution;
- event console;
- result, graph and receipt views;
- reproduction bundle.

Estimated effort: 7–10 engineering days.

## Phase 2: structured upload

Deliverables:

- signed uploads;
- quarantine and scanner;
- CSV/TSV/JSON/NDJSON parsers;
- schema mapper;
- validation and normalisation;
- download of normalised dataset;
- delete-now and TTL cleanup.

Estimated effort: 8–12 engineering days.

## Phase 3: policy laboratory

Deliverables:

- frozen policy registry;
- comparison execution;
- mutation controls for samples;
- metric comparison;
- downloadable comparison bundle.

Estimated effort: 5–7 engineering days.

## Phase 4: partner mode and launch hardening

Deliverables:

- partner request flow;
- partner authentication or access codes;
- expanded quotas;
- dashboards and alerting;
- privacy and terms pages;
- external security review;
- public launch checklist.

Estimated effort: 5–8 engineering days.

### Total indicative effort

One experienced full-stack/platform engineer with GrapheneDB C++ support:

- 28–42 engineering days for the complete public-alpha scope;
- 15–22 engineering days for a credible sample-first plus structured-upload MVP.

This estimate excludes legal approval, external penetration testing and final paper review.

---

## 29. Suggested repository structure

```text
apps/
├── public-gateway/
│   ├── src/
│   ├── tests/
│   ├── openapi.yaml
│   └── Dockerfile
├── dataset-validator/
│   ├── src/
│   └── tests/
└── worker/
    ├── entrypoint/
    ├── bundle/
    └── Dockerfile

site/
├── components/
├── pages/
├── lib/api/
├── lib/graph/
└── tests/

samples/
├── incident-two-hop-baseline/
├── incident-four-hop-hidden-chain/
├── duplicate-source-inflation/
├── independent-corroboration/
├── material-contradiction/
├── broad-retrieval-noise-trap/
├── temporal-validity-mismatch/
└── agent-memory-supersession/

schemas/
├── dataset-manifest.schema.json
├── node.schema.json
├── edge.schema.json
├── query.schema.json
├── run.schema.json
└── receipt.schema.json

infra/
├── terraform/
├── cloud-run/
└── monitoring/
```

The ChatGPT Site source may live in its own Sites workspace, but a versioned export or source-of-truth copy must be retained in the repository where supported.

---

## 30. Epics and acceptance outcomes

### Epic E1: public sample library

Outcome: A logged-out visitor can inspect, download and execute a sample dataset.

### Epic E2: secure upload and validation

Outcome: A visitor can upload an allowed structured file and receive an explicit validation result without code execution.

### Epic E3: canonical normalisation

Outcome: Every executable dataset has a downloadable canonical version and hash.

### Epic E4: isolated GrapheneDB execution

Outcome: Each run executes in a disposable environment using an immutable GrapheneDB image.

### Epic E5: evidence and receipt inspection

Outcome: A visitor can understand selected paths, lineage, contradiction and governed status.

### Epic E6: policy comparison

Outcome: The same query and dataset can be compared across frozen policies with equal budgets.

### Epic E7: reproducibility

Outcome: A downloaded bundle can reproduce a sample run on a clean machine.

### Epic E8: privacy and lifecycle

Outcome: Data is private by default, delete-now works and expiry is verifiable.

### Epic E9: partner conversion

Outcome: A qualified evaluator can request larger access with minimal personal-data collection.

### Epic E10: launch evidence

Outcome: Public claims are backed by test reports, checksums and visible limitations.

---

## 31. Definition of done

The Public Developer Evidence Lab is ready for external developer partners when:

1. the Site is publicly accessible and tested outside the owner workspace;
2. at least eight sample packs are inspectable and downloadable;
3. at least one sample and one structured upload run execute the real GrapheneDB binary;
4. every run exposes version, commit, image digest and dataset hash;
5. a policy comparison executes independent real runs;
6. the graph and compact receipt are inspectable;
7. private run artifacts cannot be accessed from another session;
8. delete-now and automatic expiry pass verification;
9. a clean local reproduction succeeds from the downloaded bundle;
10. the public licence, privacy policy, terms, limitations and security route are published;
11. no frontend secret or direct administrative database access exists;
12. all P0 release gates are evidenced.

---

## 32. Launch statement

Recommended wording:

> GrapheneDB Evidence Lab is a public, sandboxed developer alpha for testing lineage-aware evidence control, frontier-aware recovery and governed agent decisions. Visitors can inspect verified sample data or upload bounded structured datasets, run the real GrapheneDB engine, examine the resulting evidence graph and compact receipt, and reproduce the run locally. It is an experimental validation environment, not a semantic truth engine or production decision authority.

---

## 33. Open decisions

The following decisions must be resolved before implementation completes:

1. Public code licence.
2. Final immutable GrapheneDB tag.
3. Primary cloud and region for the external gateway.
4. Whether anonymous users receive a cookie/session identifier or Sign in with ChatGPT.
5. Whether raw uploaded files are included in private reproduction bundles by default.
6. Whether partner access uses access codes, GitHub OAuth or Sign in with ChatGPT.
7. Whether the first release supports server-generated embeddings or requires supplied vectors.
8. Whether public anonymised run pages are included in version 1.
9. Whether the Site source can be exported and versioned in the repository.
10. Who owns privacy, security disclosure and partner-request operations.

---

## 34. Recommended implementation decision

Build the first external preview in this order:

```text
1. Canonical schema and eight sample packs
2. Sandboxed worker and sample-run API
3. ChatGPT Site sample inspector and live result UI
4. Reproduction bundle
5. Structured upload and normalisation
6. Policy comparison
7. Partner access and larger quotas
8. Document extraction with editable graph
```

This order proves the database with transparent sample data before introducing the additional uncertainty and security surface of arbitrary uploads.
