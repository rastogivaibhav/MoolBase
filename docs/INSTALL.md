# MoolBase 0.6.0-alpha.2 — install, run, connect

Developer preview. Linux x86_64 compiled package; C++20 API names remain GrapheneDB. The server runs on loopback by default in the commands below. Native files persist. This is not enterprise GA or a shared hosted database service.

## 1. Download and verify

Download the compiled DB ZIP and examples-only ZIP with their .sha256 files into one directory. These packages do not require cloning the repository. Verify before extracting:

```sh
sha256sum -c moolbase-0.6.0-alpha.2-linux-x86_64-db.zip.sha256
sha256sum -c moolbase-0.6.0-alpha.2-examples-only.zip.sha256
unzip moolbase-0.6.0-alpha.2-linux-x86_64-db.zip
unzip moolbase-0.6.0-alpha.2-examples-only.zip
export MOOLBASE_PREFIX="$PWD/moolbase-0.6.0-alpha.2-linux-x86_64"
```

The DB ZIP contains the compiled static library, server, CLI, healthcheck, public headers, CMake package, licence notices and essential storage documentation. It contains no database implementation source, research, tests or customer examples. The examples-only ZIP contains the adapter and Python examples, not the DB implementation.

## 2. Run the examples

Prerequisites: Linux x86_64, C++20 compiler, CMake 3.16+ and Python 3. Binaries come from the existing Linux release built on Ubuntu 24.04; on older or incompatible Linux systems use the optional source build. Compiled macOS and Windows downloads are not supplied here.

```sh
cd moolbase-0.6.0-alpha.2-examples
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$MOOLBASE_PREFIX"
cmake --build build-showcase -j2
python3 examples/customer_showcase/python_memory.py
python3 examples/customer_showcase/run.py
python3 examples/customer_showcase/python_http.py --server "$MOOLBASE_PREFIX/bin/graphenedb_server"
```

Memory result: EU resolved → open/no operative answer after supersession → US resolved after independent support and explicit retirement of stale copies. Incident and supplier-report scenarios finish contested. The scripts use temporary native files and write receipts under reports/customer-showcase. The HTTP example starts an authenticated local server, verifies retry-safe ingestion and restarts it to check persisted evidence.

## 3. Connect your application

C++ applications use `find_package(GrapheneDB CONFIG REQUIRED)` and link `GrapheneDB::graphenedb`, passing the extracted DB directory as `CMAKE_PREFIX_PATH`. The examples' engine.cpp demonstrates the native evidence and runtime APIs.

Python applications can copy clients/python/graphenedb_client.py. The HTTP example calls `/v1/extractions` and `/v1/reason/runtime`. Its server key is generated temporarily; no model API key or pip package is needed. Use the native adapter for the complete correction/retirement lifecycle: HTTP does not expose that entire lifecycle. Applications establish provenance and verification and must persist prior reasoning state separately. Read status, alternatives and uncertainty together; support scores are not calibrated probabilities.

## Optional source

The source ZIP is the self-contained source from the released examples distribution: DB implementation, headers, Python client and examples. It excludes repository history, research benchmarks and the full test suite. Build from its root:

```sh
cmake -S . -B build-showcase -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_SERVER=ON
cmake --build build-showcase -j2
python3 examples/customer_showcase/python_memory.py
python3 examples/customer_showcase/python_http.py
```

For Windows use GRAPHENEDB_BUILD_SERVER=OFF and the native memory example. The browser Evidence Lab runs in a session-local WebAssembly filesystem; refreshing discards it.

## Integrity and provenance

All three ZIPs have SHA-256 files, per-file manifests and SPDX inventories. These are packaging revision 1 of unchanged release commit 6d276cacc3fd7e82ba38a364e818e91a3f1141ac, not a new engine release. The manifest records the original GitHub release archive digest. SPDX inventories describe included files; they are not security-scan results or build attestations. Apache-2.0 and applicable notices ship with the packages.
