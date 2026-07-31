# Reproducibility protocol

## Scope

This protocol reproduces the implementation and controlled evidence reported in `paper/main.tex`. It does not reproduce future external semantic baselines, production soak, enterprise security certification or global convergence claims because the paper does not make those claims.

## Frozen source

Before submission, replace the placeholder below with the immutable public tag and commit:

```text
release_tag: TO_BE_CREATED_AFTER_PUBLIC_LICENSE_AND_FINAL_GATE
source_commit: TO_BE_RECORDED
```

The arXiv version must cite an immutable tag, not a moving branch.

## Minimum environment

- Linux x86-64 is the release-blocking reference platform.
- CMake 3.16 or newer.
- C++20 compiler.
- Python 3.
- Git.
- At least 4 GB free disk for build and reports.

Record:

```bash
uname -a
cmake --version
c++ --version
python3 --version
git rev-parse HEAD
```

## Full alpha gate

From a clean clone of the frozen tag:

```bash
git clone --branch <TAG> --depth 1 \
  https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
bash scripts/run_alpha_release_gate.sh
```

Expected terminal marker:

```text
alpha_release_gate=PASS
```

The gate performs:

1. exact-head CMake configuration;
2. build of all configured targets;
3. complete CTest suite;
4. installed CMake package and unrelated consumer test;
5. dialectic intervention benchmark;
6. offline cross-dataset structural gate;
7. manifest and SHA-256 generation.

Preserve `reports/alpha-release-gate/` as the release evidence bundle.

## Controlled intervention benchmark

```bash
bash scripts/run_dialectic_intervention.sh
```

Frozen expectations:

- 700 deterministic executions;
- frontier-aware targeted policy solves all controlled hard families;
- previous completed-bundle stop exposes deeper-chain failures;
- targeted recovery visits fewer states than forced broad retrieval;
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

A differing value is not automatically a failure if compiler/platform floating-point behaviour explains a non-material presentation difference, but all frozen Boolean gates must pass.

## Offline structural benchmark

```bash
bash scripts/run_cross_dataset_local.sh offline
```

This validates representation and controller properties over committed evidence structures. It does **not** validate generated answers or public dataset accuracy.

## Optional public-data run

After dataset licence and network checks:

```bash
GRAPHENEDB_CROSS_DATASET_MODE=public \
  bash scripts/run_alpha_release_gate.sh
```

Report public-data results separately. Do not merge them with the offline structural claims unless the dataset versions, sample manifest, seed and preparation script are preserved.

## Package consumer

```bash
bash scripts/verify_developer_install.sh
```

The test must install GrapheneDB to an isolated prefix, configure an unrelated CMake project with `find_package(GrapheneDB CONFIG REQUIRED)`, link `GrapheneDB::graphenedb`, and execute the consumer.

## Paper build

Required tools: `pdflatex` and `bibtex`.

```bash
cd paper
pdflatex -interaction=nonstopmode -halt-on-error main.tex
bibtex main
pdflatex -interaction=nonstopmode -halt-on-error main.tex
pdflatex -interaction=nonstopmode -halt-on-error main.tex
```

The source archive submitted to arXiv should contain only the paper source and necessary figures/bibliography, not build directories or database artifacts.

## Independent reproduction report

External validators should record:

- release tag and commit;
- OS, architecture, compiler and CMake versions;
- clean-clone command;
- alpha-gate result;
- failed test names and logs, if any;
- intervention summary;
- package-consumer result;
- deviations from the reference environment;
- whether source was modified.

## Integrity

Publish alongside the release:

- source archive;
- install archive for the supported reference platform;
- `.sha256` file;
- `.manifest.json` file;
- alpha-gate evidence archive;
- paper source archive.

Checksums provide artifact integrity and inventory, not author identity. Signing should be added when release-signing infrastructure exists.
