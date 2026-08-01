# Evidence Lab site

This zero-build frontend can be deployed conventionally or used as the visual and API-contract reference for ChatGPT Sites.

## Gateway configuration

Set the HTTPS gateway before loading `app.js`:

```html
<script>
  window.GRAPHENEDB_EVIDENCE_LAB_API = "https://evidence.your-domain.com";
</script>
```

`config.example.js` is a deployment template. Never place a permanent gateway, cluster or registry credential in browser code.

For local development:

```bash
python3 -m http.server 8088 -d apps/evidence_lab/site
```

The UI always distinguishes:

```text
LIVE GRAPHENEDB EXECUTION
RECORDED REFERENCE — BACKEND NOT USED
```

## ChatGPT Sites

Build the public Site against the `/v1/public` contract and configure the gateway base URL as a server-side Site value. Add the published Site HTTPS origin to `EVIDENCE_LAB_ALLOWED_ORIGINS` during deployment.

The Site must preserve:

- sample inspection and download;
- bounded structured upload;
- visible upload-security result;
- dataset hash and exact GrapheneDB release;
- live/recorded distinction;
- evidence graph and compact receipt;
- reproduction-bundle download;
- delete-now;
- privacy and prohibited-data notice.

## Public policy gate

Before publishing, replace every `REPLACE_WITH_...` value in:

- `privacy.html`;
- `security.html`.

The policy pages are templates, not legal approval. They must match the actual operator, hosting region, retention, subprocessors and security-reporting route.
