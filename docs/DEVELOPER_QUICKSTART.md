# Start with one example

**Recommended build: 0.6.0-audit.2**, the corrected evaluation build from merged source `0f19727b80007e875dea62dca7ab50e281d008e6`. The original `v0.6.0-alpha.2` release is historical and unchanged. Use audit.2 for parity with this Lab. This is a developer preview, not enterprise GA.

**Download → run the memory example → inspect its receipt → integrate.** On a compatible Linux x86_64 machine with the tools below already installed, this is the five-minute path; first-time tool installation and compilation may take longer. No model key, pip package or repository clone is required.

## 1. Download

You need Linux x86_64, a C++20 compiler, CMake 3.16+, Python 3 and unzip. The compiled DB was built with GCC 13.3.0 on Ubuntu 24.04. For incompatible systems, macOS or Windows, use the optional source path below.

Save these four files into one empty directory:

- [Compiled DB ZIP](https://moolbase.rasvai.com/downloads/moolbase-0.6.0-audit.2-linux-x86_64-db.zip) and [its SHA-256 file](https://moolbase.rasvai.com/downloads/moolbase-0.6.0-audit.2-linux-x86_64-db.zip.sha256).
- [Examples ZIP](https://moolbase.rasvai.com/downloads/moolbase-0.6.0-audit.2-examples-only.zip) and [its SHA-256 file](https://moolbase.rasvai.com/downloads/moolbase-0.6.0-audit.2-examples-only.zip.sha256).

## 2. Run one example

Open a terminal in that directory:

```sh
sha256sum -c moolbase-0.6.0-audit.2-linux-x86_64-db.zip.sha256
sha256sum -c moolbase-0.6.0-audit.2-examples-only.zip.sha256
unzip moolbase-0.6.0-audit.2-linux-x86_64-db.zip
unzip moolbase-0.6.0-audit.2-examples-only.zip
export MOOLBASE_PREFIX="$PWD/moolbase-0.6.0-audit.2-linux-x86_64"
cd moolbase-0.6.0-audit.2-examples
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$MOOLBASE_PREFIX"
cmake --build build-showcase -j2
python3 examples/customer_showcase/python_memory.py
```

Expected: EU fulfilment is resolved initially. Superseding only the original record leaves EU selected because other active sources remain. More US support opens the result; independent corroboration selects US, and stale copies are explicitly retired. Reopening the native files preserves the evidence bundle. These are synthetic fixtures, not customer outcomes.

## 3. Inspect the receipt

```sh
python3 -m json.tool reports/customer-showcase/python-memory.json
```

Read each state's `status`, `answer`, `targets`, `evidence`, `receipt` and `runtimeContract`. `answer` is a target ID, not a truth verdict. Compare supporting families and opposition; scores are policy strengths, not probabilities. `resolved` means the configured evidence policy was satisfied. The export is a reduced adapter receipt, not a complete native runtime receipt.

The runner uses temporary native database files and saves JSON receipts. Persistent applications choose their own data directory and separately persist and restore prior reasoning state.

## 4. Integrate

Start with the embedded C++ adapter for the complete correction/retirement flow. `MoolBase` is the product name; `GrapheneDB` is the compatible C++/CMake API name. Python in this example only drives the native executable.

```cmake
find_package(GrapheneDB CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE GrapheneDB::graphenedb)
```

Read `examples/customer_showcase/engine.cpp` and the [minimal observation/receipt exchange](API_EXAMPLE.md). Applications supply targets, evidence roles, provenance families and verification. MoolBase does not infer those from prose or authenticate sources.

If HTTP fits your prototype, run the optional pilot example:

```sh
python3 examples/customer_showcase/python_http.py --server "$MOOLBASE_PREFIX/bin/graphenedb_server"
```

It starts an authenticated local server, ingests evidence through `/v1/extractions`, evaluates `/v1/reason/runtime` and checks evidence after a server restart. HTTP does not expose the complete correction/retirement lifecycle and does not restore prior reasoning state. It is not a shared hosted service or a published MCP server. See [integration details](AGENT_INTEGRATION.md).

## Optional source and other platforms

The [source ZIP](https://moolbase.rasvai.com/downloads/moolbase-0.6.0-audit.2-source.zip) contains the complete pinned source, including tests and historical validation fixtures. Building the DB is optional for the recommended Linux path. Extract it and build from its root:

```sh
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=OFF -DGRAPHENEDB_BUILD_BENCH=OFF -DGRAPHENEDB_BUILD_SERVER=ON
cmake --build build-showcase --target moolbase_customer_showcase graphenedb_server -j2
python3 examples/customer_showcase/python_memory.py
```

On Windows, set `GRAPHENEDB_BUILD_SERVER=OFF`, build only `moolbase_customer_showcase`, and pass your executable path with `--binary` (for Visual Studio, normally `build-showcase/Release/moolbase_customer_showcase.exe`). HTTP is POSIX-only. No compiled macOS or Windows downloads are supplied here.

## Build identity and maturity

The DB ZIP includes the library, server, CLI, headers, CMake package and essential storage docs. The examples ZIP includes the native adapter and Python runners. Each ZIP has SHA-256, per-file manifest and SPDX inventory links in [Downloads](https://moolbase.rasvai.com/index.html#developer). These identify bytes and contents; they are not signatures, security scans or build attestations. Packages include Apache-2.0 and applicable notices.

The artifact build was tested at `b01c22334a08e1ad26dee0cff21a37b04ef22c82`; its tree is identical to merged source `0f19727b80007e875dea62dca7ab50e281d008e6`. Historical release assets are unchanged. See [audit.2 notes](RELEASE_v0.6.0-audit.2.md), [capabilities](CAPABILITIES.md) and [validation boundaries](https://moolbase.rasvai.com/VALIDATION.html). Production-scale measurements and external customer outcomes are not established.

## Optional pinned repository checkout

```sh
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
git checkout 0f19727b80007e875dea62dca7ab50e281d008e6
```

Then follow the optional source build above.
