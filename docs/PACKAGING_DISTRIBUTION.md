# Packaging And Distribution

GrapheneDB installs as a CMake package named `GrapheneDB`.

Installed artifacts include:

- `GrapheneDB::graphenedb` CMake target
- public headers under `include/graphene`
- C ABI header `include/graphene/c_api.h`
- static/shared library artifacts
- `graphenedb_cli` when CLI builds are enabled
- docs and example source files
- `GrapheneDBConfig.cmake`
- `GrapheneDBConfigVersion.cmake`
- `GrapheneDBTargets.cmake`

## Consumer Contract

External CMake projects should use:

```cmake
find_package(GrapheneDB CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE GrapheneDB::graphenedb)
```

The package config propagates required thread dependencies through the exported target.

## Verification Gate

Windows:

```powershell
.\scripts\verify_package_install.ps1
```

POSIX:

```bash
scripts/verify_package_install.sh
```

The verification script:

1. Builds GrapheneDB.
2. Installs it to a local prefix.
3. Configures an external consumer using only `find_package(GrapheneDB)`.
4. Builds and runs the consumer.
5. Exercises `put_extraction()`, lattice validation, and durable close.

This is a GA packaging gate. A release candidate should not ship unless this verification passes on each supported target platform.

## Release Manifest

Release package scripts generate:

- the install archive
- `<archive>.sha256`
- `<archive>.manifest.json`

The manifest records package SHA-256, package size, version, install prefix name, and SHA-256 hashes for each installed file. This is not a cryptographic signature, but it gives release reviewers and downstream consumers a reproducible artifact inventory before signing infrastructure exists.
