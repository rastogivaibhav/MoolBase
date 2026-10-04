This Lab uses corrected evaluation commit `0f19727b80007e875dea62dca7ab50e281d008e6`. Use the [current installation guide](DEVELOPER_QUICKSTART.md) and its 0.6.0-audit.2 packages for exact replay parity. The original alpha.2 release remains unchanged.

# Integrate MoolBase from a coding agent

MoolBase stores evidence and exposes competing hypotheses, opposition and decision receipts. Applications establish provenance and verification; the database does not authenticate a source or discover independence from prose.

## Supported newcomer checkout

For exact parity with the corrected Lab, use commit `0f19727b80007e875dea62dca7ab50e281d008e6`. The first example needs only the two packages in [Install](DEVELOPER_QUICKSTART.md). Repository checkout is optional:

```bash
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
git checkout 0f19727b80007e875dea62dca7ab50e281d008e6
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_SERVER=ON
cmake --build build-showcase --target moolbase_customer_showcase graphenedb_server -j2
python3 examples/customer_showcase/python_memory.py --binary build-showcase/moolbase_customer_showcase
python3 examples/customer_showcase/python_http.py --server build-showcase/graphenedb_server
```

Prerequisites: Git, Python 3, CMake 3.16+, C++20 compiler; the HTTP server is POSIX. No pip package or model API key is required. Python import setup and process teardown are included in the example files. Receipts are written beneath `reports/customer-showcase/`. Start with the single memory example in [Install](DEVELOPER_QUICKSTART.md); the HTTP run is optional. See an [actual observation/receipt exchange](API_EXAMPLE.md) and the [capability matrix](CAPABILITIES.md).

## Select the existing integration surface

| Need | Surface | Runnable starting point |
|---|---|---|
| Correct and retire evidence, carry prior decision state | Embedded C++ `GrapheneDB` and `CompleteHypoKoshRuntime` | `python_memory.py` runs the complete native adapter; read `engine.cpp` to embed it |
| Ingest evidence and get a runtime receipt over HTTP | Pilot server `/v1/extractions` and `/v1/reason/runtime` | `python_http.py` starts a real server and uses Python standard-library HTTP |
| Generate hypothetical proposals | Python client `reason_hypokosh()` | This is proposal generation, not full runtime decision evaluation |
| Interactive explanation | Evidence Lab browser worker | Session-local WebAssembly, not a shared database service |

The published Site currently has no MCP server. An agent can run these examples with shell access, or call the pilot HTTP API through its own tool adapter. Browser tool registration is optional and is not a portable Claude/Grok/OpenAI connector.

## Native lifecycle input contract

`engine.cpp` accepts observations through `demo_add`; its CLI reads TSV. `scenarios.json` and the Python memory runner show complete input construction.

| Field | Contract |
|---|---|
| `id` | Unique nonblank event ID, at most 80 UTF-8 bytes |
| `content` | Nonblank observation, at most 1000 UTF-8 bytes |
| `family` | Nonblank established provenance family, at most 80 UTF-8 bytes |
| `target` | Integer 0 or 1, indexing the two supplied hypotheses |
| `kind` | `support`, `refute`, `revoke`, or `supersede` |
| `verified` | Browser JSON boolean; native TSV 0 or 1; verification is externally supplied |
| `retire` | Active prior ID for revoke/supersede; empty for support/refute |

Browser inputs reject NUL characters. TSV fields cannot contain tabs, newlines or NULs. The demo supports 100 active observations and 200 history events. Lifecycle changes remain available at the active limit; reset after exporting at the history limit. Duplicate evidence IDs and invalid retirement references fail without inserting replacement evidence. Atomic lifecycle writes use `BatchInput::delete_node_ids` and `put_batch`, preserving previous evidence when the WAL commit fails.

## Interpret the response before taking action

Render the selected answer together with `status`, alternatives, support/opposition, uncertainty and receipt. `open` means no committed answer in this adapter. A selected alternative can still be contested or provisional; support scores are not calibrated probabilities. Inspect truncation and verification flags. Keep correlated copies in one provenance family. Explicitly retire known stale derived copies too.

## Persistence and errors

Native database files persist on disk. The example keeps `PriorEpistemicState` in its adapter process; embedding applications must serialize/version and restore that state themselves. HTTP runtime requests create fresh runtime state and do not preserve that prior state. The HTTP example verifies persisted evidence after a process restart, not restart persistence of decision history. Browser refresh discards files.

The CLI refuses an existing database directory. Parse errors exit 2 with row diagnostics; rejected operations exit 1. Browser operations return `{ok:false,error:...}`; display the error and preserve the last successful result. HTTP uses authenticated requests, validation errors, and 409 for conflicting idempotency replay. Preserve caller evidence on errors; never automatically convert unverified or inferred evidence into verified observation.

## Verify an integration

Run `verify.py` and `verify_cli.py` against your compiled adapter. Run `verify_wasm.cjs` with matching native receipts for a browser build. `verify_playwright.mjs` exercises the actual worker, desktop/mobile flows, malformed inputs, capacity and failed/partial lifecycle WAL writes. These tests establish the bounded examples, not production-scale or semantic truth claims.
