# Customer showcase validation — 2026-10-01

Scope is the three customer demos and adapter. This is not a new production release gate.

- Native C++20 Release build passed.
- Three primary scenarios contain 25 guided evidence updates and 31 total states including initial and reopened states.
- Ten native replay executions checked family deduplication, explicit retirement, revision/decommitment, memory recommitment, reproducibility, quoted/Unicode JSON content, database reopen and rejected unknown retirement.
- Native CLI refuses existing database directories, so sample runs cannot overwrite a user's database.
- Actual engine compiled with Emscripten 3.1.74. Browser payload: approximately 817 KiB WASM plus 85 KiB loader before transfer compression.
- All 31 guided WebAssembly states matched the native status, selected answer, bundle hash, native events and family counts. Two additional reopen calls checked that invalid retirement leaves the bundle unchanged. The virtual filesystem contained the actual `graphene.wal`.
- DOM integration tests ran the page controls and worker adapter against the actual WebAssembly engine: scenario switching, next/full replay, reset, close/reopen, custom form, rejection and HTML injection handling passed.
- JavaScript syntax and Git whitespace checks passed.

No visual browser pass was available in this environment: browser installation failed. DOM tests do not establish rendering quality across browsers. Optional WebMCP registration and valid/invalid execution were checked in an emulated API context; validation in a supported browser WebMCP context was unavailable. Neither limitation blocks the requested demo.

## Expected final states

| Scenario | Operative target | Status | Native behavior exposed |
|---|---|---|---|
| Incident | Database connection exhaustion | contested | Resolution, revision and decommitment before alternative selection |
| Memory | US fulfilment | resolved | Revision, decommitment, recommitment and resolution |
| Conflict | 21-day delivery | contested | Resolution, revision and decommitment before alternative selection |

These fixtures are realistic synthetic replays, not production customer data. They are manually typed and use simple fixed vectors and caller-supplied verification certificates. No natural-language extraction, source authentication, independence discovery, calibrated confidence or production-scale performance was evaluated. Browser state is session-local. Native file replay is tested; process-restart persistence of the adapter's prior reasoning state is not implemented.

Reproduce:

```bash
python3 examples/customer_showcase/run.py
python3 examples/customer_showcase/verify.py
node examples/customer_showcase/verify_wasm.cjs /absolute/wasm-output reports/customer-showcase
```

Use the README to build the native executable and the browser module first.
