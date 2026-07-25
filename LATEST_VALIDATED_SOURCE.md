# Latest validated GrapheneDB source

This branch preserves `master` and records the latest validated source package prepared after the storage-v3 fixes and rerun.

Source archive: `graphenedb_latest_source_only.zip`

SHA-256: `5ea0402959bae8b9e7384d9db4ed9ac51c1d5576ae16f198761e695388290cf6`

The archive contains 241 source, test, configuration, documentation, client, and build-definition files. Generated build directories, runtime databases, WAL files, and benchmark outputs are excluded.

To use it:

```bash
unzip graphenedb_latest_source_only.zip -d graphenedb_latest
cd graphenedb_latest
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DGRAPHENEDB_BUILD_TESTS=ON
cmake --build build-release -j 2
ctest --test-dir build-release --output-on-failure
```
