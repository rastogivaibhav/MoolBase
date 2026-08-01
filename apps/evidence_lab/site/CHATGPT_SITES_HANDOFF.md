# ChatGPT Sites handoff — GrapheneDB Evidence Lab

Use this only after the HTTPS Evidence Lab gateway has passed the exact-head live gate.

## Required inputs

Replace these values before starting the Sites build:

```text
GATEWAY_HTTPS_ORIGIN=https://REPLACE_WITH_GATEWAY_HOST
EXPECTED_GRAPHENEDB_COMMIT=REPLACE_WITH_EXACT_40_CHARACTER_COMMIT
PUBLIC_RELEASE_VERSION=v0.6.0-alpha.1
OPERATOR_NAME=REPLACE_WITH_OPERATOR_NAME
PRIVACY_CONTACT=REPLACE_WITH_PRIVACY_CONTACT
SECURITY_CONTACT=REPLACE_WITH_SECURITY_CONTACT
```

Do not put Kubernetes credentials, registry credentials, API keys or permanent bearer tokens in the Site project or browser JavaScript.

## Files to provide to Sites

Provide the complete contents of:

```text
apps/evidence_lab/site/
```

The implementation reference is:

```text
index.html
styles.css
app.js
privacy.html
security.html
config.example.js
README.md
```

## Exact Sites prompt

```text
@Sites Deploy this existing project as the public GrapheneDB Evidence Lab website.

Use the attached files from apps/evidence_lab/site as the source of truth. Preserve the existing information architecture, typography, responsive behaviour, evidence graph, upload flow, live-versus-recorded labels, receipt display, execution events, download flow, delete-session control, privacy page and security page.

Audience:
- database engineers
- AI infrastructure engineers
- agent-framework developers
- research engineers
- potential technical partners

This is a public developer alpha and technical validation instrument. It must never describe GrapheneDB as a semantic truth engine, managed public database, production decision authority, enterprise-GA service or state-of-the-art benchmark winner.

Connect every API request to this external HTTPS gateway:

GATEWAY_HTTPS_ORIGIN

Required API contract:
- GET /v1/public/health
- POST /v1/public/sessions
- DELETE /v1/public/sessions/{session_id}
- GET /v1/public/samples
- GET /v1/public/samples/{sample_id}
- GET /v1/public/samples/{sample_id}/download
- POST /v1/public/uploads
- GET /v1/public/datasets/{dataset_id}
- POST /v1/public/runs
- GET /v1/public/runs/{run_id}
- GET /v1/public/runs/{run_id}/bundle

Send X-Session-ID only for session-owned dataset, run and bundle requests. Do not create or expose any permanent credential in browser code. Do not persist uploaded datasets or run results in Sites storage; the external gateway is the sole system of record for the one-hour anonymous session.

Preserve these exact trust labels:
- LIVE GRAPHENEDB EXECUTION
- RECORDED REFERENCE — BACKEND NOT USED

Before showing a result as live, require:
- live equals true
- run_mode equals live_graphenedb
- receipt.graphene_executed equals true
- graphenedb_commit equals EXPECTED_GRAPHENEDB_COMMIT
- graphenedb_version equals PUBLIC_RELEASE_VERSION or the exact deployed release label

Upload behaviour:
- accept only JSON, NDJSON/JSONL, the canonical GrapheneDB dataset ZIP, or the nodes + edges + queries file set already supported by the project
- show the prohibited-data warning before file selection
- never claim that a rejected upload was executed
- display the returned security report and schema validation result
- never echo credentials or rejected file contents into analytics, logs or error pages

Privacy and security:
- replace every placeholder with OPERATOR_NAME, PRIVACY_CONTACT and SECURITY_CONTACT
- state the one-hour retention and immediate Delete my session control
- state that users must not upload confidential, personal, medical, financial, credential-bearing or unauthorised third-party data
- do not collect marketing details, accounts or payments
- do not add non-essential tracking cookies or third-party analytics; use only the standard Sites analytics provided by the platform

Visual behaviour:
- retain the premium technical dark interface
- remain usable on mobile, tablet and desktop
- keep raw dataset hashes, exact version/commit, execution events and compact receipt visible
- keep sample downloads and reproduction-bundle downloads available
- do not replace real execution stages with simulated percentages or fake progress

Build a private preview first. Do not deploy or publish yet. Report any runtime incompatibility, blocked cross-origin request or unsupported browser API without silently replacing it with mock data.
```

Substitute the literal `GATEWAY_HTTPS_ORIGIN`, `EXPECTED_GRAPHENEDB_COMMIT`, `PUBLIC_RELEASE_VERSION`, `OPERATOR_NAME`, `PRIVACY_CONTACT` and `SECURITY_CONTACT` values in the prompt before sending it.

## Private-preview gate

The private preview must pass all of these before deployment:

1. Health shows `backend_mode=kubernetes`, `live_backend_configured=true`, `security_profile=public` and `malware_scan_required=true`.
2. Both verified samples are visible and downloadable.
3. A sample run returns the live label, exact commit and immutable worker image reference.
4. A safe structured upload displays `security.passed=true` before execution.
5. A malformed reference and a credential-bearing upload are rejected visibly.
6. The evidence graph, compact receipt and execution events render without truncating the trust fields.
7. The reproduction ZIP downloads successfully.
8. Delete my session succeeds and old dataset/run URLs no longer work.
9. Browser developer tools show no permanent credential, cluster address, registry token or upload content in analytics calls.
10. Privacy and security pages contain no `REPLACE_WITH_` placeholders.

## Publishing sequence

1. Save a Sites version without deploying.
2. Test the private preview against the production gateway.
3. Copy the preview origin into `EVIDENCE_LAB_ALLOWED_ORIGINS` temporarily if Sites uses a separate preview origin.
4. Run the full private-preview gate.
5. Deploy the Site.
6. Copy the production Sites origin.
7. Set the gateway CORS allowlist to the exact production Sites origin and remove temporary preview origins that are no longer needed.
8. Run:

```bash
python scripts/validate_evidence_lab_public.py \
  https://REPLACE_WITH_GATEWAY_HOST \
  --expected-commit REPLACE_WITH_EXACT_40_CHARACTER_COMMIT \
  --timeout 240
```

9. Open the published Site in a logged-out private browser and repeat one sample run, one upload run, one bundle download and delete-now.
10. Preserve the Site URL, gateway URL, exact commit, immutable image digests and validation output as release evidence.

A Site URL alone is not release evidence. Publication is complete only when the external gateway live gate and the logged-out browser acceptance both pass.
