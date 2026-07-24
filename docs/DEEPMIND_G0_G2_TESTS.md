# G0 and G2 Foundation Tests

Status: implemented research gate for `GDB-DH-AM-1`; this is not a claim of
DeepMind review or endorsement.

## What is implemented

### G0: reproducible foundation run

`scripts/deepmind_foundation_manifest.py` creates a fail-closed manifest before
gate execution. Its immutable identity binds:

- a clean source commit and a hash of every tracked source file;
- benchmark and D0-generator source hashes;
- storage and extraction format versions;
- the production image reference, image ID digest, and documented UID;
- the prepared D0 dataset, explicit seed, pinned D1 commit, and D1 tree hash;
- embedding, generation, prompt, retrieval-policy, hardware,
  tool-permission, and external-version checksums.

The identity is protected by a canonical SHA-256 lock. Finalization rechecks
the live source and input hashes, records hashes and sizes for raw evidence,
and can claim G0 only when every required result is exactly `true`.

`scripts/run_deepmind_foundation.sh` is the clean-checkout orchestrator. Its
first gate command creates the manifest. It then runs release CTest, OpenAPI
route parity, package-consumer verification, pinned-image non-root execution,
formal D0/G2, and both `git diff --check` and a final clean-status check.

The runner deliberately does not build or mutate inputs. Prepare these before
the run and place `RUN_DIR` and `D0_EVIDENCE_DIR` outside the source checkout.
Supply an already-built image by immutable image ID.

### G2: deterministic dialectic correctness

`graphenedb_deepmind_causal_suite` generates D0 from an explicit seed and
evaluates it through the embedded `DialecticEngine`. Formal mode requires:

- 5,000 graphs and 20,000 episodes;
- 20,000 unique deterministic query vectors (four blinded variants per graph);
- graph boundary coverage from 2 through 200 nodes and 1 through 8 roots;
- 40% multi-root, 20% contradiction, 20% temporal, 20% all-source
  hyperedge, and 20% no-evidence cases;
- chains, forks, joins, diamonds, cycles, and disconnected evidence;
- equal and adversarial near-tie roots;
- all epistemic origins, missing provenance, stale/future/malformed temporal
  facts, supersession, safe/unsafe compressed paths, incomplete all-source
  requirements, and exact abstention cases.

Truth is serialized separately from the database. It includes exact roots,
admissible paths, forbidden edges, provenance findings, temporal violations,
hyperedge violations, contradiction expectations, and abstention
expectations. The database never receives oracle labels.

Formal pass is fail-closed:

- root-set recall = 1.000;
- admissible-path recall = 1.000;
- inadmissible-path acceptance = 0;
- false-promotion rate = 0;
- contradiction-detection recall >= 0.99;
- correct abstention >= 0.99;
- temporal, hyperedge, provenance-truth, and budget violations = 0;
- the suite completes within the configured wall-clock budget.

Every episode preserves raw JSON prediction evidence, latency, warnings,
visited states, opposition rounds, and zero-valued token/tool/cost accounting
for this embedded, model-free track. A TSV completion journal makes retries
resumable without duplicating completed episode IDs.

## Commands

Build and run a diagnostic:

```bash
cmake -S . -B build-g2 \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_BENCH=ON
cmake --build build-g2 --target graphenedb_deepmind_causal_suite -j2
./build-g2/graphenedb_deepmind_causal_suite \
  --output-dir /tmp/graphenedb-d0-diagnostic \
  --seed 20260724 \
  --graphs 25 \
  --queries-per-graph 4
```

Prepare a formal D0 input before creating the G0 manifest:

```bash
./build-g2/graphenedb_deepmind_causal_suite \
  --output-dir /evidence/d0-pinned \
  --seed 20260724 \
  --graphs 5000 \
  --queries-per-graph 4 \
  --prepare-only
```

Then invoke the foundation runner from a clean committed checkout:

```bash
RUN_ID=foundation-001 \
RUN_DIR=/evidence/foundation-001 \
D0_EVIDENCE_DIR=/evidence/d0-pinned \
D1_CORPUS=/inputs/postmortems \
D1_COMMIT=0ed8afb0f8cfd83a34bbda270943ebdbf9661062 \
IMAGE_REF=graphenedb-server:pinned \
IMAGE_DIGEST=sha256:<image-id> \
HARDWARE_PROFILE=/inputs/hardware.json \
EXTERNAL_VERSIONS=/inputs/external-versions.json \
bash scripts/run_deepmind_foundation.sh
```

## Negative contracts

CTest includes two gate-specific contract tests:

- `graphenedb_deepmind_g0_manifest_tests` rejects dirty source, missing hashes,
  absent start timestamps, underspecified completion, and identity tampering.
- `graphenedb_deepmind_g2_harness_tests` verifies exact diagnostic results,
  raw-evidence resume idempotency, and rejection of an undersized formal run.

G0 cannot pass in a dirty development worktree by design. Commit the intended
source revision and run the foundation runner from a clean checkout.
