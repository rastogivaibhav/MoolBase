# API examples

The `examples/` folder contains runnable C++ examples.

## Coding memory

Use GrapheneDB to store why a module was designed a certain way:

```cpp
NodeInput adr;
adr.content = "ADR-007: split checkout pricing module because migration latency created retry storms.";
adr.vector = embedding;
adr.root = true;
adr.metadata = {{"type", "adr"}, {"module", "checkout-pricing"}};
```

Run:

```bash
./build-release/graphenedb_api_coding_memory
```

## Incident memory

Store a root cause and symptom with a causal edge:

```cpp
EdgeInput e;
e.from = root;
e.to = symptom;
e.role = EdgeRole::Causal;
e.confidence = 0.94;
```

Run:

```bash
./build-release/graphenedb_api_incident_memory
```

## Team brain

Store customer context, current decisions, and superseded decisions:

```cpp
EdgeInput e;
e.from = current_decision;
e.to = old_decision;
e.role = EdgeRole::Supersedes;
```

Run:

```bash
./build-release/graphenedb_api_team_brain
```
