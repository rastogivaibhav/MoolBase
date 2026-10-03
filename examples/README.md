# MoolBase examples

**Start with the [released customer examples](https://github.com/rastogivaibhav/MoolBase/tree/v0.6.0-alpha.2/examples/customer_showcase):** customer-memory correction, incident investigation and conflicting reports. Follow the [developer quickstart](../docs/DEVELOPER_QUICKSTART.md) and [agent integration guide](../docs/AGENT_INTEGRATION.md). These examples live on `v0.6.0-alpha.2`; the default branch also contains frozen research work.

The examples below are additional embedded API demonstrations.

Build examples with the default release preset:

```bash
./scripts/build_release.sh
```

Run all examples:

```bash
./scripts/run_examples.sh
```

## Files

- `graphene_uniqueness_demo.cpp` — compares vector-only, graph-only, and Graphene causal-memory retrieval.
- `api_coding_memory.cpp` — stores an architecture decision and bug memory.
- `api_incident_memory.cpp` — stores an incident/root-cause memory and metadata search.
- `api_team_brain.cpp` — stores customer context, team decision, and supersession memory.
- `api_contradiction_supersession.cpp` — stores contradictory hypotheses and superseded/current decisions.
- `graphene_lattice_memory.cpp` — stores lattice coordinates, validates neighbor bonds, and retrieves with lattice propagation.
