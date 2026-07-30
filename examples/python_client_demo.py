#!/usr/bin/env python3
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "clients", "python"))
from graphenedb_client import GrapheneDBClient

client = GrapheneDBClient(api_key=os.environ.get("GRAPHENEDB_API_KEY"))
print(client.version().data)
created = client.put_node(
    "deployment introduced a memory leak",
    source="demo",
    idempotency_key="demo-deployment-memory-leak",
)
print(created.data)
print(client.get_node(created.data["id"]).data)
print(client.search("memory leak after deployment").data)
