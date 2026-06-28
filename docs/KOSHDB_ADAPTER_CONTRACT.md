# KoshDB / LLM-Kosh adapter contract

Recommended v1 adapter methods:

```text
ingest_memory(content, vector, signature, metadata)
retrieve_memory(query_vector, k)
retrieve_causal_bundle(query_vector, signature, mode)
upsert_document_chunk(document_id, chunk_id, content, vector, metadata)
delete_memory(memory_id)
explain_retrieval(bundle)
snapshot()
backup(path)
restore(path)
compact()
health_check()
```

GrapheneDB should be used as a sidecar memory core first, not as a full replacement for KoshDB.
