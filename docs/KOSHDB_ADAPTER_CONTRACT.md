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

## Local Gate

The local runnable adapter gate is:

Windows:

```powershell
.\scripts\run_kosh_adapter_gate.ps1
```

POSIX:

```bash
scripts/run_kosh_adapter_gate.sh
```

That gate validates the TSV interchange path, metadata preservation, causal linking, retrieval, and reopen durability. It does not validate a live external KoshDB or LLM-Kosh runtime.
