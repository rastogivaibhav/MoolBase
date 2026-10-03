# Reproducibility protocol

## Scope

This protocol reproduces the implementation and controlled evidence reported in `main.tex`. It does not reproduce future semantic baselines, production soak, enterprise security certification, global convergence, million-node model-world claims, or implementation-outcome learning because the paper does not make those claims.

## Frozen source identifiers

Before submission, replace all placeholders in `arxiv_metadata.json`, `ARTIFACT_MANIFEST.md`, and `main.tex` with:

```text
release_tag: PENDING IMMUTABLE PUBLIC TAG
source_commit: PENDING EXACT COMMIT
archive_url: PENDING PUBLIC ARCHIVAL URL
software_doi: PENDING OR NOT AVAILABLE
```

The paper must cite an immutable release, not a moving branch.

## Minimum environment

- Linux x86-64 is the release-blocking reference platform.
- CMake 3.16 or newer.
- A C++20 compiler.
- Python 3.
- Git.
- `pdflatex`, `bibtex`, `latexmk`, and `chktex` for the paper gate.
- At least 4 GB free disk for build and reports.

Record:

```bash
uname -a
cmake --version
c++ --version
python3 --version
pdflatex --version | head -n 1
latexmk -v | head -n 2
git rev-parse HEAD
git status --short
```

## Full alpha gate

From a clean clone of the frozen tag:

```bash
git clone --branch <TAG> --depth 1 \
  https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
bash scripts/run_alpha_release_gate.sh
```

Expected terminal marker:

```text
alpha_release_gate=PASS
```

The gate is expected to perform exact-head CMake configuration, build, complete CTest, installed-package consumer verification, the intervention benchmark, the offline structural gate, and artifact manifest/checksum generation. Preserve the resulting release-evidence directory unchanged.

## Controlled intervention benchmark

```bash
bash scripts/run_dialectic_intervention.sh
```

Frozen expectations:

- 700 deterministic executions: seven families, five policies, twenty signature variants;
- frontier-aware targeted policy solves all controlled hard families;
- previous completed-bundle stop exposes deeper-chain failures;
- targeted recovery visits fewer states than broad retrieval;
- broad retrieval fails the controlled noise trap;
- frontier-aware targeted recovery passes the noise trap.

Expected hard-family aggregate:

| Policy | Accuracy | Mean cycles | Mean visited states |
|---|---:|---:|---:|
| no cycle | 0.0% | 0.00 | 2.67 |
| previous targeted stop | 66.7% | 1.83 | 10.17 |
| forced targeted | 100.0% | 3.00 | 16.00 |
| forced broad | 83.3% | 3.00 | 34.00 |
| frontier-aware targeted | 100.0% | 3.00 | 16.00 |

The topology-preserving variants are repeated deterministic mechanism tests, not independent natural-language samples. A differing presentation value is not automatically a failure when platform floating-point formatting explains it, but every frozen Boolean gate must pass.

## Offline structural benchmark

```bash
bash scripts/run_cross_dataset_local.sh offline
```

This validates representation and controller properties over committed evidence structures. It does **not** validate generated answers or public-dataset accuracy.

## Optional public-data mode

After dataset licence, network, version, sample-manifest, and seed checks:

```bash
GRAPHENEDB_CROSS_DATASET_MODE=public \
  bash scripts/run_alpha_release_gate.sh
```

Report public-data results separately. Do not merge them into the offline structural claim unless preparation scripts and exact samples are frozen and independently reviewable.

## Installed-package consumer

```bash
bash scripts/verify_developer_install.sh
```

The test must install GrapheneDB to an isolated prefix, configure an unrelated CMake project with `find_package(GrapheneDB CONFIG REQUIRED)`, link `GrapheneDB::graphenedb`, and execute the consumer.

## Paper build and source validation

```bash
make -C paper check
```

This runs metadata/citation validation, compiles the manuscript with `latexmk`, and performs a non-blocking `chktex` pass.

After replacing all human/release placeholders:

```bash
python3 scripts/check_arxiv_package.py --paper-dir paper --strict
```

The strict gate fails if submission placeholders remain.

## arXiv source package

```bash
make -C paper arxiv
```

Expected outputs:

```text
paper/build/arxiv/graphenedb-arxiv-source.tar.gz
paper/build/arxiv/graphenedb-arxiv-source.sha256
```

Inspect the archive before upload:

```bash
tar -tzf paper/build/arxiv/graphenedb-arxiv-source.tar.gz
sha256sum -c paper/build/arxiv/graphenedb-arxiv-source.sha256
```

The archive intentionally contains the `.bib` and generated `.bbl`, plus only the TeX source and a manifest required for transparent compilation. It excludes the full repository and all build residue.

## Independent reproduction report

External validators should record:

- release tag, source commit, and archive checksum;
- OS, architecture, compiler, CMake, Python, and TeX versions;
- clean-clone command;
- alpha-gate result;
- failed test names and logs, if any;
- intervention summary and machine-readable result path;
- structural gate result;
- installed-package consumer result;
- paper build and package result;
- deviations from the reference environment;
- whether source was modified.

## Integrity and identity

Publish alongside the release:

- source archive;
- supported-platform install archive;
- SHA-256 files;
- machine-readable manifest;
- alpha-gate evidence archive;
- paper source archive;
- archival URL/DOI when available.

Checksums prove byte identity and inventory, not author identity. Signed tags or attestations should be added when release-signing infrastructure exists.
