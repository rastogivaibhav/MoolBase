# MoolBase by RASVAI

**A persistent evidence and provenance database for AI agents — with optional reasoning when you need it.**

[![CI](https://github.com/rastogivaibhav/graphenedb_v1/actions/workflows/ci.yml/badge.svg)](https://github.com/rastogivaibhav/graphenedb_v1/actions/workflows/ci.yml)
[![Alpha release gate](https://github.com/rastogivaibhav/graphenedb_v1/actions/workflows/alpha-release-gate.yml/badge.svg)](https://github.com/rastogivaibhav/graphenedb_v1/actions/workflows/alpha-release-gate.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

**Store what your agent knows, where it came from, what conflicts with it, and how that state changes over time.**

You can use **MoolBase Core** as a database on its own.

When the application needs more, add:

- **Hypothesis Engine** — preserve and compete alternative explanations.
- **Dialectic Engine** — challenge a current decision and reopen it when evidence changes.
- **Epistemic Receipts** — keep an inspectable record of the evidence and revision path.

You do **not** need to adopt the reasoning stack to use the database.

**Status:** developer alpha for research and controlled pilots. Not enterprise GA and not a semantic truth engine.

> **Naming:** MoolBase is the provisional public product identity for the system historically developed as **GrapheneDB**. Existing APIs, benchmark hashes, receipts, papers and implementation identifiers retain their historical names during migration. See [ADR-0001](docs/adr/0001-moolbase-public-product-identity.md).

**Start here:** [See database examples](#what-can-you-build-with-moolbase-core) · [Build](#build-from-source) · [Optional reasoning](#optional-reasoning) · [Run the proof](#run-the-proof) · [Docs](#documentation)

---

## Why MoolBase Core

A normal database can persist records.

An agent-memory system can retrieve useful context.

MoolBase Core is designed to persist **evidence-aware state**:

- the observation or claim;
- where it came from;
- how it relates to other evidence;
- whether another item supports, contradicts or supersedes it;
- vector and metadata representations;
- snapshots and history;
- causal/graph relationships;
- the context needed to explain why something was retrieved.

The core database can already be useful without any hypothesis or dialectic reasoning.

### Familiar database mental model

```text
Your agent / application
        │
        ├── put_node(...)
        ├── put_edge(...)
        ├── put_extraction(...)
        │
        ├── vector_search(...)
        ├── metadata_search(...)
        ├── causal_search(...)
        │
        ├── get_node(...)
        ├── get_edge(...)
        └── snapshot(...)
                │
                ▼
        ┌─────────────────────┐
        │    MoolBase Core    │
        │                     │
        │ Evidence / nodes    │
        │ Relationships       │
        │ Vectors + metadata  │
        │ Provenance          │
        │ Contradictions      │
        │ Supersession        │
        │ Snapshots / history │
        └─────────────────────┘
```

The current implementation API still uses the historical C++ name `GrapheneDB`.

---

## What can you build with MoolBase Core?

These examples use the database directly — no Hypothesis Engine or Dialectic Engine required.

### Incident memory

Store a symptom, its root cause and the source that linked them:

```text
"API timeout spike after release 2.3"
           ▲
           │ causal · source=postmortem
           │
"ingress timeout dropped from 30s to 3s"
```

Then use `causal_search(...)` to retrieve the incident together with the causal path and provenance.

→ [Incident memory example](examples/api_incident_memory.cpp)

### Contradiction and supersession

Keep a stale hypothesis in history instead of overwriting it:

```text
Current root cause ── contradicts ──► Old hypothesis
Current action     ── supersedes ──► Old action/state
```

Query `status=current` or `status=superseded` while retaining both states for audit.

→ [Contradiction + supersession example](examples/api_contradiction_supersession.cpp)

### Coding-agent memory

Connect an architecture decision to the failure that motivated it:

```text
ADR-007: split checkout pricing
        │
        └── evidence / causal context ──► BUG-331: retry storm
```

A coding agent can retrieve **why** the architecture changed, not only the latest code state.

→ [Coding memory example](examples/api_coding_memory.cpp)

### Team decision memory

Persist customer context, the current decision and what it replaced:

```text
Customer context
      ↓
Current decision ── supersedes ──► Old decision
```

This gives team agents a queryable decision history without flattening old and new states into one summary.

→ [Team brain example](examples/api_team_brain.cpp)

### Run the examples

```bash
./scripts/build_release.sh
./scripts/run_examples.sh
```

See [`examples/README.md`](examples/README.md) for the full example catalogue and walkthroughs.

---

## Optional reasoning

MoolBase Core is the foundation. The reasoning capabilities sit **on top**.

```mermaid
flowchart TB
    A["Your agent / application"]

    subgraph CORE["MoolBase Core — usable on its own"]
      M["Persistent evidence + provenance"]
      DB["Nodes · relationships · vectors · metadata<br/>contradictions · supersession · snapshots"]
      M --> DB
    end

    subgraph OPTIONAL["Optional reasoning"]
      H["Hypothesis Engine<br/>preserve + compete alternatives"]
      D["Dialectic Engine<br/>challenge · reopen · reassess"]
      H --> D
    end

    R["Epistemic receipt<br/>inspectable decision provenance"]

    A --> CORE
    CORE -->|optional| H
    D -->|reopen with new evidence| CORE
    D --> R
    CORE -->|audit / provenance only| R
```

### Adoption levels

| Use only what you need | What it gives you |
|---|---|
| **MoolBase Core** | persistent evidence, provenance, graph/vector/metadata search, contradictions, supersession and snapshots |
| **+ Receipts / audit** | inspectable evidence and decision provenance |
| **+ Hypothesis Engine** | multiple plausible explanations remain explicit |
| **+ Dialectic Engine** | challenge, reopen and revise a governed decision when the evidence warrants it |

This progressive model is intentional. A developer can stop at the database layer.

### When the full reasoning loop is enabled

```text
Evidence + provenance
        ↓
MoolBase Core
        ↓
Evidence bundle / verification / admissibility
        ↓
Hypothesis Engine
        ↓
Initial governed convergence or abstention
        ↓
Dialectic Engine
        ↓
   Reopen needed?
     ↙       ↘
   No         Yes
   ↓           ↓
Terminal    targeted evidence expansion
state          ↓
   ↑        MoolBase Core
   └───────────┘
        ↓
Epistemic receipt
```

The observable event sequence can include:

```text
HypothesisSet → Decision → Challenge → Reopen → Revision → Terminal
```

This is decision provenance, not persisted private model chain-of-thought.

Historical implementation names remain **GrapheneDB**, **HypoKosh** and **Dialectical Model Worlds (DWM)**.

---

## Developer interaction surfaces

| Layer | What you can do today | Current surfaces |
|---|---|---|
| **Database** | open the store, ingest evidence, query vectors/metadata/lattice/causal memory, inspect and validate | C++ `GrapheneDB`; limited C API; HTTP/Python ingest + search |
| **Reasoning** | generate hypotheses, run governed reasoning, invoke challenge/reopen | C++ `HypoKoshEngine`; `CompleteHypoKoshRuntime`; CLI; HTTP/Python reasoning |
| **Control** | configure retrieval/reasoning bounds, verification and capability switches | `RuntimeOptions`; `DialecticOptions`; `PathVerifier` |
| **Audit + state** | build receipts, inspect event history, persist/audit world state, validate provenance and back up | `CompactEpistemicReceipt`; `ModelWorld`; admin/validation surfaces |

The C++ API is currently the richest integration surface.

---

## Build from source

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRAPHENEDB_BUILD_TESTS=ON \
  -DGRAPHENEDB_BUILD_SERVER=OFF \
  -DGRAPHENEDB_BUILD_BENCH=ON \
  -DGRAPHENEDB_BUILD_EXAMPLES=ON

cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The historical `GRAPHENEDB_*` build options and namespaces remain intentionally stable during the naming transition.

For the packaged alpha path, see [Developer quickstart](docs/DEVELOPER_QUICKSTART.md).

---

## Minimal API

### C++ database

```cpp
#include "graphene/db.hpp"

graphene::GrapheneDB db;
graphene::DBOptions options;
options.dimension = 16;

auto status = db.open("/tmp/moolbase", options);
```

### Optional governed reasoning

```cpp
#include "graphene/hypokosh_runtime.hpp"

graphene::CompleteHypoKoshRuntime runtime(db);
graphene::RuntimeOptions options;

auto result = runtime.reason(query, signature, options);
```

### Python / HTTP

```python
from graphenedb_client import GrapheneDBClient

client = GrapheneDBClient(api_key="development-key")

client.put_node(
    "deployment introduced a memory leak",
    source="incident-review",
)

print(client.search("memory leak after deployment").data)
```

---

## Run the proof

If you want to evaluate the reasoning mechanism rather than only the database API:

```bash
git clone https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
python3 scripts/run_flagship_perturbations_v1.py
```

This rebuilds and replays the frozen flagship proof, verifies its canonical mechanism receipt, runs five pre-registered adversarial perturbations and writes machine-readable receipts.

**No hosted model or API key is required.**

Canonical flagship mechanism receipt:

```text
36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef
```

See [Independent flagship reproduction](docs/INDEPENDENT_REPRODUCTION.md).

---

## Evidence and validation

Current reproducibility includes:

- frozen flagship proof + adversarial perturbations;
- exact-head CI and alpha-release gates;
- controlled intervention benchmarks;
- paper/system conformance checks;
- installed-package consumer tests;
- distribution integrity, checksums and SPDX 2.3 SBOM generation.

The current scientific programme is implementing an architecture ablation:

```text
B0  baseline
G0  MoolBase Core
G1  + Hypothesis Engine
G2  + Dialectic Engine
```

The score lock remains closed until the production execution boundaries and clean UNSCORED runs are proven.

See [MoolBase Lab & Market Program](https://github.com/rastogivaibhav/graphenedb_v1/issues/25).

---

## Maturity and limitations

**Developer alpha.**

Current limitations include:

- G0/G1/G2 production ablation is still being completed;
- structural stability is not semantic truth;
- no global asymptotic-stability proof exists for an unbounded model world;
- generic parsing is bounded and is not general natural-language understanding;
- public API surfaces still need further consolidation;
- long-duration production-hardware soak remains open;
- MoolBase domain/package migration is not final;
- independent reproductions and adopters are still required before GA claims.

---

## Documentation

- [Examples](examples/README.md)
- [Developer quickstart](docs/DEVELOPER_QUICKSTART.md)
- [Independent reproduction guide](docs/INDEPENDENT_REPRODUCTION.md)
- [Reasoning modes and receipts](docs/REASONING_MODES_AND_RECEIPTS.md)
- [Distribution security](docs/DISTRIBUTION_SECURITY.md)
- [Naming ADR — MoolBase public product identity](docs/adr/0001-moolbase-public-product-identity.md)
- [Agent Memory Is Not Enough](docs/articles/agent-memory-is-not-enough.md)
- [Changelog](CHANGELOG.md)
- [Support](SUPPORT.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)

---

## Contribute or challenge it

The most valuable contribution right now is an **independent reproduction, failure case, adversarial scenario or credible criticism**.

- Read [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
- For setup or usage help, see [SUPPORT.md](SUPPORT.md).
- Reproduce the [flagship proof](docs/INDEPENDENT_REPRODUCTION.md).
- Submit a failure case or technical critique through [GitHub Issues](https://github.com/rastogivaibhav/graphenedb_v1/issues).
- See the [public reproduction request](https://github.com/rastogivaibhav/graphenedb_v1/issues/38).

For security vulnerabilities, follow [SECURITY.md](SECURITY.md) rather than opening a public issue.

---

## Citation

If you use MoolBase/GrapheneDB in research, see [CITATION.cff](CITATION.cff).

## Maintainer

Maintained by [@rastogivaibhav](https://github.com/rastogivaibhav).

## License

Apache-2.0. See [LICENSE](LICENSE), [NOTICE](NOTICE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

**MoolBase by RASVAI** — *Store the evidence. Preserve the alternatives. Know why belief changed.*
