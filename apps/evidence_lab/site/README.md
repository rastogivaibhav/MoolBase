# Evidence Lab site

This zero-build static frontend can be deployed as a conventional site or used as the visual/reference implementation for ChatGPT Sites.

Set the gateway before loading `app.js`:

```html
<script>
  window.GRAPHENEDB_EVIDENCE_LAB_API = "https://api.example.com";
</script>
```

For local development:

```bash
python3 -m http.server 8088 -d apps/evidence_lab/site
```

The UI always distinguishes `LIVE GRAPHENEDB EXECUTION` from `RECORDED REFERENCE — BACKEND NOT USED`.
