# MoolBase customer showcase

Three runnable, realistic synthetic workloads demonstrate when provenance-aware agent memory is useful. They are examples, not customer case studies or production benchmarks. The adapter calls the actual embedded C++ database and `CompleteHypoKoshRuntime`; the web interface compiles the same adapter and engine to WebAssembly.

## Start with Python

Build once using the commands below, then run:

```bash
python3 examples/customer_showcase/python_memory.py
```

Expected progression: EU resolved → EU remains selected after retiring only the original → open as competing support grows → US resolved after independent corroboration and explicit retirement of stale copies. The script saves all native receipts and verifies file reopen. It starts a disposable native database and uses no model or API key.

For HTTP ingestion, full runtime evaluation and process restart, also build the server:

```bash
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_SERVER=ON -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF
cmake --build build-showcase --target graphenedb_server -j2
python3 examples/customer_showcase/python_http.py
```

The HTTP example starts an authenticated loopback server with a temporary key and database, ingests a causal chain, verifies retry-safe ingestion, evaluates `/v1/reason/runtime`, and checks the bundle after restarting the server process. It cleans up its process and database. HTTP currently does not expose the native adapter’s supersession operation or carry prior reasoning state between requests. Use the native example for the complete lifecycle. See [Agent integration guide](../../docs/AGENT_INTEGRATION.md).

## Run the native examples

From the repository root, with C++20, CMake 3.16+ and Python 3:

```bash
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF \
  -DGRAPHENEDB_BUILD_SERVER=OFF
cmake --build build-showcase --target moolbase_customer_showcase -j2
python3 examples/customer_showcase/run.py
```

The script writes JSON receipts to `reports/customer-showcase/`. It uses temporary databases, closes and reopens each database, checks that replay preserves the final evidence bundle and answer, then cleans up. Native writes use the engine's default fsync behavior. To keep a database, invoke the executable directly with a **new** directory:

```bash
./build-showcase/moolbase_customer_showcase /tmp/new-memory-db events.tsv \
  'Use EU fulfilment for this account' 'Use US fulfilment for this account'
```

Rows are tab-separated: evidence ID, description, family, zero-based target (0 or 1), action (`support`, `refute`, `revoke`, `supersede`), supplied certificate (0 or 1), prior evidence ID to retire, reserved empty column. The fixture runner shows how to convert the JSON examples into this format. Do not put tabs or newlines in TSV fields.

## Customer use cases and actual expected behavior

| Scenario | Useful customer workflow | Observed behavior | Simpler alternative |
|---|---|---|---|
| Incident | Investigating changing telemetry with multiple plausible causes | Deployment initially selected; rollback contradiction clears the commitment; independent traces select connection exhaustion; final state remains contested | Log search plus a human investigation is enough when decisions need no governed provenance |
| Agent memory | Retiring stale account facts and tracking an authenticated update | EU memory initially selected; retiring only the original leaves other active sources; competing support opens the result; independent US evidence and explicit stale-copy retirement resolve US | Last-write-wins is simpler for one authoritative preference source |
| Conflicting reports | Procurement or research with duplicated, corrected and unverified sources | Seven-day claim initially selected; supplier correction causes decommitment; independent records select 21 days; final state remains contested | A source list is enough if users manually compare and reconcile claims |

The exact fixtures are in `scenarios.json`. Repeated notices intentionally share family IDs. Different IDs are meaningful only when the application has established genuinely independent provenance; the database does not infer independence from natural language. Supersession/revocation retirement is explicit: applications must retire known derived copies too. Contradictory evidence does not automatically disappear when another target leads.

## Integrate the production APIs

Read `engine.cpp` for a complete small adapter:

1. Open `GrapheneDB` with your vector dimension; reserve node zero because transition receipts use zero for no target.
2. Write hypothesis roots and observation nodes with `put_node()`.
3. Add typed observed support/contradiction edges with `put_edge()`. Record `source_id`, canonical `evidence_family_id`, and lifecycle metadata.
4. Supply your external `PathVerifier`, or leave evidence unverified. The fixture verifier reads caller-provided certificates; it is not a semantic truth classifier.
5. Call `CompleteHypoKoshRuntime::reason()` with bounded retrieval and recovery options.
6. Carry `PriorEpistemicState` forward between evidence updates. In a durable application, persist that state separately; this example keeps it in adapter memory across close/reopen.
7. Render `status`, alternatives, uncertainty and native receipt events together. An operative selection is not necessarily a resolved commitment. Support strength is a policy score, not a calibrated probability.

## Hosted demo boundaries

The Site hosts the UI and actual compiled database engine. The engine runs in a browser worker using a session-local virtual filesystem. It is not a shared, hosted database service. Close/reopen replays real database files inside that filesystem, but reload discards the session. No localStorage persistence, remote LLM, API key or invented JavaScript reasoning results are used. Sessions allow 100 active observations and 200 total history events, and are bounded to two hypotheses. It is developer alpha for controlled demos, not a production deployment recommendation.

## Reproduce the browser build

Use Emscripten 3.1.74, then run:

```bash
bash examples/customer_showcase/build_wasm.sh /absolute/output/directory
```

The output includes `moolbase.js` and `moolbase.wasm`. Serve them over HTTP with the Site's `worker.js`, `app.js`, `scenarios.json`, HTML and CSS. The downloadable code bundle contains the adapter, fixtures, complete database source, license and a minimal CMake build. It runs independently after extraction; the linked repository includes the wider product and validation history.

### Lifecycle and bounded sessions

The Evidence Lab permits 100 active observations and 200 total evidence events.
Revocation and supersession remain available at the active limit. At the history
limit, download the receipt and reset. Retrieval budgets cover every node and
path within these limits, including audit records, so duplicate volume cannot
hide a later contradiction. Evidence retirement, its audit record and the
replacement are committed together through `BatchInput::delete_node_ids` and
`put_batch`. Failed WAL commits leave both storage and adapter state unchanged.
This extends the C++ batch API using existing WAL records; it does not change
the durable storage format.

The worker rejects malformed types, embedded NULs and excessive UTF-8 byte
lengths before calling the engine. CLI numeric fields use complete, bounded
integer parsing; malformed rows exit 2 with a row number before database writes.

Run browser regressions with Playwright and Chromium:

```sh
npm install playwright@1.51.1
npx playwright install chromium
node examples/customer_showcase/verify_playwright.mjs /absolute/path/to/site/dist
python3 examples/customer_showcase/verify_cli.py build-evaluator/moolbase_customer_showcase
```

The Playwright script serves the supplied site assets locally, exercises desktop
and mobile flows, validates real worker inputs and capacity limits, and injects
failed and partial WAL writes into the actual compiled engine.

## Current distribution boundaries

The audit.2 DB ZIP contains compiled binaries, headers and the CMake package. The examples ZIP contains the adapter and Python runners. The optional source ZIP is complete pinned source, including tests and historical validation fixtures; it excludes Git repository history only. Historical package documentation can reflect an earlier packaging boundary. Use the [current installation guide](https://moolbase.rasvai.com/INSTALL.html) and [package errata](https://moolbase.rasvai.com/PACKAGE_ERRATA.html) for the current distributions. Published archive bytes and checksums remain immutable.
