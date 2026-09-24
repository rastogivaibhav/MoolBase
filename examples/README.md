# MoolBase examples

The examples are organised around **developer problems**, not internal research components.

Start with the database-core examples. They show that MoolBase is useful without enabling the optional reasoning engines.

Build examples:

```bash
./scripts/build_release.sh
```

Run the database-core examples:

```bash
./scripts/run_examples.sh
```

---

## Database core examples

### Incident memory

**File:** [`api_incident_memory.cpp`](api_incident_memory.cpp)

Use MoolBase to persist:

- an incident symptom;
- a root-cause record;
- metadata such as service and incident ID;
- an observed causal edge;
- the source of that causal relationship.

The example then uses:

```cpp
db.metadata_search("service", "gateway");
db.causal_search(query, signature, QueryMode::Empirical);
```

Use this pattern for incident copilots, SRE memory, postmortem retrieval and operational knowledge.

---

### Contradiction and supersession

**File:** [`api_contradiction_supersession.cpp`](api_contradiction_supersession.cpp)

Use MoolBase when new evidence should **change the current state without deleting history**.

The example stores:

```text
current root cause ── contradicts ──► stale hypothesis
current action     ── supersedes ──► stale action/state
```

It also demonstrates:

```cpp
db.metadata_search("status", "current");
db.metadata_search("status", "superseded");
db.causal_search(...);
db.validate();
```

Use this pattern for changing facts, incident revisions, policy history and any agent that must explain why an earlier state is no longer current.

---

### Coding-agent memory

**File:** [`api_coding_memory.cpp`](api_coding_memory.cpp)

Persist an architecture decision together with the bug or incident that motivated it:

```text
ADR-007: split checkout pricing
          │
          └── causal evidence ──► BUG-331: retry storm
```

A coding agent can later retrieve the **reason behind the architecture**, rather than treating the current code structure as context-free fact.

Use this pattern for ADR memory, codebase agents, migration assistants and long-running engineering copilots.

---

### Team decision brain

**File:** [`api_team_brain.cpp`](api_team_brain.cpp)

Store:

- customer or product context;
- the current team decision;
- the decision it superseded;
- metadata such as `status=current` and `status=superseded`.

Use this pattern for product decisions, team memory, project agents and persistent organisational context.

---

### Vector + graph causal retrieval

**File:** [`graphene_uniqueness_demo.cpp`](graphene_uniqueness_demo.cpp)

Compares vector-only, graph-only and causal-memory retrieval.

Use it to understand why MoolBase Core is not intended to be only a vector store.

---

### Lattice-backed memory

**File:** [`graphene_lattice_memory.cpp`](graphene_lattice_memory.cpp)

Demonstrates lattice coordinates, neighbor validation and retrieval with lattice propagation.

This is a lower-level storage/retrieval capability and is not required for the common evidence/provenance use cases above.

---

## Optional reasoning examples

Once the database-core model is clear, move to the optional reasoning layer.

### Governed reasoning runtime

**File:** [`hypokosh_runtime.cpp`](hypokosh_runtime.cpp)

Demonstrates the current historical C++ runtime API:

```cpp
ModelWorld world;
CompleteHypoKoshRuntime runtime(db, &world);

RuntimeOptions options;
const auto result = runtime.reason(query, signature, options);
```

The runtime can add:

- competing-hypothesis handling;
- governed convergence or abstention;
- opposition/challenge;
- bounded reopen/recovery;
- persistent model-world events;
- inspectable receipts.

---

### Epistemic flagship

**Files:**

- [`epistemic_flagship_demo.cpp`](epistemic_flagship_demo.cpp)
- [`epistemic_flagship_perturbations.cpp`](epistemic_flagship_perturbations.cpp)

These are research/reproducibility examples rather than the recommended first developer entry point.

Use them when you want to inspect the full evidence → hypotheses → challenge → reopen/revision → receipt mechanism.

---

## Suggested learning path

```text
1. api_incident_memory.cpp
        ↓
2. api_contradiction_supersession.cpp
        ↓
3. api_coding_memory.cpp or api_team_brain.cpp
        ↓
4. hypokosh_runtime.cpp
        ↓
5. epistemic_flagship_demo.cpp
```

The first three steps require understanding only **MoolBase Core**.

The reasoning runtime comes later, when your application needs competing hypotheses or belief revision.
