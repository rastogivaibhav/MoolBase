# Codex Notes

Use `AGENTS.md` as the primary repository instruction file.

GrapheneDB v0.6.0-rc1 is an embedded C++20 causal/physical-lattice database with an optional controlled-pilot HTTP server. Preserve the embedded core as the source of truth. Keep server concurrency, request admission, retry safety, graceful checkpoint shutdown, durable-format compatibility, and reverse-proxy-only deployment constraints intact.

The current branch is a **pilot release candidate**, not unrestricted public GA. The default task is validation and stabilization, not new Kosh/dialectic features.

Run:

```bash
bash scripts/run_pilot_rc1_gate.sh
```

Before public distribution, complete external 24h/72h soak, actual OCI/SBOM/vulnerability scanning, resolve remaining long-profile performance/harness issues, and replace the placeholder licence.
