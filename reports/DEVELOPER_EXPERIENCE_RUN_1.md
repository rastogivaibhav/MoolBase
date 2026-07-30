# Developer experience validation — Run 1

Date: 30 July 2026

## Purpose

Reduce the first-run path to one command and prove that GrapheneDB can be installed and consumed through its exported CMake package rather than only built inside its own source tree.

## Added developer assets

- `scripts/developer_quickstart.sh`;
- `scripts/developer_quickstart.ps1`;
- `scripts/verify_developer_install.sh`;
- `docs/DEVELOPER_QUICKSTART.md`;
- `examples/installed_consumer/main.cpp`;
- `examples/installed_consumer/CMakeLists.txt`;
- `.github/workflows/developer-experience.yml`;
- a five-minute entry point at the top of `README.md`.

## Executed local validation

Environment:

- Linux x86_64;
- CMake 3.31.6;
- GNU C++ 14.2.0;
- no prior build cache in the verification directory;
- server, tests, benchmarks and examples disabled for the install build.

The verification performed:

1. clean CMake configuration;
2. build of all installable targets;
3. installation into an isolated temporary prefix;
4. configuration of an unrelated consumer with `find_package(GrapheneDB CONFIG REQUIRED)`;
5. linking against `GrapheneDB::graphenedb`;
6. execution of the consumer against a disposable database.

Observed consumer output:

```text
GrapheneDB consumer example passed
nodes=2 edges=1
top_result="checkout failures increased" score=1
```

Result:

```text
PASS: clean build, install, find_package and consumer execution succeeded.
```

## Defect found during validation

The first verifier revision built only the `graphenedb` library target. `cmake --install` then failed because the installation manifest also contained the CLI executable. The verifier was corrected to build all configured installable targets before installation.

## Claim boundary

The executed local test used the provided source snapshot at `e8829d184b28904e8908a63f3901b1609d1e8327` plus the newly committed consumer and verification assets. The package contract exercised by the consumer is unchanged on the current branch, but this is not represented as a clean clone of the private latest head.

A dedicated GitHub Actions workflow was added to perform the exact latest-head Ubuntu clean-install test and Windows first-run demo. Its first run terminated before checkout and exposed zero workflow steps, consistent with the existing hosted-runner infrastructure failure affecting the other workflows. Therefore latest-head hosted execution remains pending; the workflow configuration itself is committed and ready to rerun when the runner infrastructure becomes available.
