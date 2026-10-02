# Customer showcase validation — 2026-10-02

Scope is the three customer demos and adapter. This is not a new production release gate.

- Native C++20 Release build passed.
- Three primary scenarios contain 25 guided evidence updates and 31 total states including initial and reopened states.
- Ten native replay executions checked family deduplication, explicit retirement, revision/decommitment, memory recommitment, reproducibility, quoted/Unicode JSON content, database reopen and rejected unknown retirement.
- Native CLI refuses existing database directories, so sample runs cannot overwrite a user's database.
- Actual updated engine compiled with Emscripten 3.1.74.
- All 31 guided WebAssembly states matched the native status, selected answer, bundle hash, native events and family counts. Two additional reopen calls checked that invalid retirement leaves the bundle unchanged. The virtual filesystem contained the actual `graphene.wal`.
- DOM integration tests ran the page controls and worker adapter against the actual WebAssembly engine: scenario switching, next/full replay, reset, close/reopen, custom form, rejection and HTML injection handling passed.
- JavaScript syntax and Git whitespace checks passed.

Actual Playwright 1.51.1 tests passed in Chromium 134.0.6998.35 at 1440×900 and 390×844. All 10 regression groups passed: guided scenarios, receipt downloads, custom supersession, target preservation, reopen, failed reset consistency, ZIP download and mobile layout; real worker validation rejected 11 malformed inputs without changing the database; 100 active and 200 history capacity boundaries passed; 60 duplicate observations did not hide a later material contradiction; the actual browser WASM engine preserved the original evidence after failed and partial WAL writes during supersession. A mobile overflow discovered by Playwright was corrected.

The complete native CTest suite passed 55/55, including the new atomic lifecycle regression with append/write/fsync failures, replay, historic snapshots, validation, compaction and deletion of both endpoints of a shared edge. Seven malformed CLI numeric rows exited normally with code 2 and row diagnostics before database creation; a pre-existing database was preserved.

Optional WebMCP registration was previously checked in an emulated API context; a supported browser WebMCP context was not part of this run. Browser coverage here is Chromium, not a claim about every browser.

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
python3 examples/customer_showcase/verify_cli.py build-evaluator/moolbase_customer_showcase
node examples/customer_showcase/verify_playwright.mjs /absolute/site/dist
```

Use the README to build the native executable and the browser module first.

## Newcomer onboarding update — 2026-10-02

The Python memory example passed against the native engine (EU resolved → open → US resolved). The Python HTTP example passed authenticated extraction replay and preserved the evidence bundle across a real server process restart. HTTP limitations are documented separately from the native lifecycle. The updated Lab is tested with Playwright, including before/after explanations for repeated source families. Site access remains owner-only pending an explicit audience change.
