# MoolBase customer showcase

Three runnable, realistic synthetic workloads demonstrate when provenance-aware agent memory is useful. They are examples, not customer case studies or production benchmarks. The adapter calls the actual embedded C++ database and `CompleteHypoKoshRuntime`; the web interface compiles the same adapter and engine to WebAssembly.

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
| Agent memory | Retiring stale account facts and tracking an authenticated update | EU memory initially selected; supersession produces an open state; independent US evidence produces recommitment; final US state resolved | Last-write-wins is simpler for one authoritative preference source |
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

The Site hosts the UI and actual compiled database engine. The engine runs in a browser worker using a session-local virtual filesystem. It is not a shared, hosted database service. Close/reopen replays real database files inside that filesystem, but reload discards the session. No localStorage persistence, remote LLM, API key or invented JavaScript reasoning results are used. Evidence is capped at 100 records and bounded to two hypotheses. It is developer alpha for controlled demos, not a production deployment recommendation.

## Reproduce the browser build

Use Emscripten 3.1.74, then run:

```bash
bash examples/customer_showcase/build_wasm.sh /absolute/output/directory
```

The output includes `moolbase.js` and `moolbase.wasm`. Serve them over HTTP with the Site's `worker.js`, `app.js`, `scenarios.json`, HTML and CSS. The downloadable code bundle contains the adapter, fixtures, complete database source, license and a minimal CMake build. It runs independently after extraction; the linked repository includes the wider product and validation history.
