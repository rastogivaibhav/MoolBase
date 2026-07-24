# GrapheneDB Python Pilot Client

The pilot client uses only Python's standard library.

```python
from graphenedb_client import GrapheneDBClient

client = GrapheneDBClient("http://127.0.0.1:8080", api_key="development-key")
client.put_extraction({
    "schema_version": 1,
    "source_id": "incident-42",
    "nodes": [
        {"external_id": "root", "content": "cache saturation", "role": "root"},
        {"external_id": "symptom", "content": "checkout latency", "role": "symptom"},
    ],
    "relations": [{
        "from_external_id": "root",
        "to_external_id": "symptom",
        "origin": "observed",
        "role": "causal",
        "confidence": 0.95,
        "evidence_id": "postmortem-42",
    }],
})
created = client.put_node(
    "checkout deployment increased memory use",
    source="incident-system",
    idempotency_key="incident-42-deployment",
)
node_id = created.data["id"]
print(client.lattice_neighbors(node_id).data)
reasoned = client.reason_dialectic(
    "why did checkout latency increase?",
    mode="empirical",
    as_of="2026-07-24T12:00:00Z",
)
print(reasoned.data["synthesis"])

hypotheses = client.reason_hypokosh(
    "why did checkout latency increase?",
    tenant_id="merchant-risk",
    use_active_policy=True,
)
for proposal in hypotheses.data["proposals"]:
    assert proposal["origin"] == "hypothetical"
    print(proposal["statement"], proposal["discriminating_tests"])
```

Set `GRAPHENEDB_API_KEY` to avoid putting the key in source code.

`Idempotency-Key` is supported for `POST /v1/nodes` and `POST /v1/facts`. Repeating the same request returns the original node with `idempotent_replay=true`. Reusing a key with different content returns HTTP 409.

`POST /v1/extractions` is atomic and uses stable `source_id` plus
`external_id` values for durable replay safety. An identical replay returns
HTTP 200 without new writes; a changed node, relation, or evidence payload
returns HTTP 409.

The experimental governed-learning methods are
`record_learning_episode()`, `evaluate_learning_policies()`,
`decide_learning_policy()`, `current_learning_policy()`, and
`quarantine_learning_episode()`. Policy evaluation is read-only. Promotion and
rollback require explicit approval provenance. These APIs select bounded
retrieval policies; they do not train model weights.
