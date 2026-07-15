# GrapheneDB Python Pilot Client

The pilot client uses only Python's standard library.

```python
from graphenedb_client import GrapheneDBClient

client = GrapheneDBClient("http://127.0.0.1:8080", api_key="development-key")
created = client.put_node(
    "checkout deployment increased memory use",
    source="incident-system",
    idempotency_key="incident-42-deployment",
)
node_id = created.data["id"]
print(client.lattice_neighbors(node_id).data)
```

Set `GRAPHENEDB_API_KEY` to avoid putting the key in source code.

`Idempotency-Key` is supported for `POST /v1/nodes` and `POST /v1/facts`. Repeating the same request returns the original node with `idempotent_replay=true`. Reusing a key with different content returns HTTP 409.
