# MoolBase developer quickstart

This guide is for a developer who wants a first successful GrapheneDB run before studying FiberBundle, the Lyapunov critic or the model-world architecture.

## What you need

- Git access to this repository;
- CMake 3.16 or newer;
- a C++20 compiler;
- Linux or macOS for the shell quickstart;
- Windows PowerShell plus a C++ build toolchain for the PowerShell quickstart.

The reasoning runtime is an experimental developer alpha, not an enterprise-GA decision system.

## 1. Clone the supported newcomer examples

```bash
git clone --branch v0.6.0-alpha.2 --single-branch \
  https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
```

The repository is public. The example checkout is separate from the frozen research work on the default branch. For complete Python lifecycle and HTTP examples, follow [Agent integration](AGENT_INTEGRATION.md).

## 2. Run the first demo

Linux or macOS:

```bash
bash scripts/developer_quickstart.sh
```

Windows PowerShell:

```powershell
.\scripts\developer_quickstart.ps1
```

The script configures a small Release build, compiles the complete HypoKosh runtime demo and runs a disposable incident example. Expected output includes:

```text
status=...
primary_node=...
bundle_hash=...
stability=...
opposition=...
```

The exact governed status may remain provisional because the runtime deliberately retains residual uncertainty.

## 3. Understand the first example

The demo stores a small causal chain:

```text
release changed pool timeout
        ↓
connection pool exhausted
        ↓
checkout failures
```

It then asks the complete runtime to reason from the observed checkout-failure vector. The result exposes the selected target, governed status, immutable FiberBundle hash, stability score and opposition score.

Start with `examples/hypokosh_runtime.cpp`. It is intentionally small and uses an in-process temporary database.

## 4. Verify installation as a real dependency

Run:

```bash
bash scripts/verify_developer_install.sh
```

This performs a clean five-stage check:

1. configure GrapheneDB with no prior build cache;
2. build all installable targets;
3. install headers, library, CLI and CMake package files into a temporary prefix;
4. build a separate application using `find_package(GrapheneDB CONFIG REQUIRED)`;
5. run that external consumer.

Set `GRAPHENEDB_KEEP_VERIFY_DIR=1` to retain the temporary installation for inspection.

## 5. Use GrapheneDB from another CMake project

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyGrapheneApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(GrapheneDB CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE GrapheneDB::graphenedb)
```

Configure the consumer with the install prefix:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/graphenedb/install
cmake --build build
```

A complete consumer is available in `examples/installed_consumer`.

## 6. Persist a compact answer receipt

Do not persist every complete FiberBundle by default. Reduce the runtime result to a deterministic receipt:

```cpp
#include "graphene/epistemic_receipt.hpp"

HypoKoshRuntimeResult result = runtime.reason(query, signature, options);
CompactEpistemicReceipt receipt =
    build_compact_epistemic_receipt(result);
```

The receipt preserves selected path IDs, evidence/source/derivation lineage, bundle and evidence references, governed status, energy and uncertainty while avoiding duplicate storage of source data, indexes and full recursive workspaces.

## 7. Run the complete alpha release gate

On an authenticated Linux/macOS clone:

```bash
bash scripts/run_alpha_release_gate.sh
```

The gate performs:

1. clean exact-head Release configuration;
2. build of all configured targets;
3. full CTest execution;
4. installed-package external consumer verification;
5. the controlled dialectic intervention benchmark;
6. the offline cross-dataset structural gate;
7. immutable manifest and SHA-256 generation.

Use `GRAPHENEDB_CROSS_DATASET_MODE=public` only on a networked machine when you intentionally want the downloaded public-data run.

## 8. Run the standard validation suite

```bash
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure
```

## Choose the right entry point

| Goal | Start here |
|---|---|
| See it run | `scripts/developer_quickstart.sh` or `.ps1` |
| Learn the embedded API | `examples/installed_consumer/main.cpp` |
| Explore governed reasoning | `examples/hypokosh_runtime.cpp` |
| Persist compact results | `include/graphene/epistemic_receipt.hpp` |
| Validate package installation | `scripts/verify_developer_install.sh` |
| Run the complete alpha gate | `scripts/run_alpha_release_gate.sh` |
| Reproduce recursion experiments | `scripts/run_dialectic_intervention.sh` |
| Run structural benchmarks | `docs/benchmarks/PORTABLE_CROSS_DATASET_VALIDATION.md` |
| Study the critic | `docs/LYAPUNOV_CRITIC_V1.md` |

## Current platform boundary

The embedded library is intended to build across supported C++ platforms. The controlled-pilot HTTP server is POSIX-oriented and should remain disabled on Windows. Use `-DGRAPHENEDB_BUILD_SERVER=OFF` for the first developer build on every platform.
