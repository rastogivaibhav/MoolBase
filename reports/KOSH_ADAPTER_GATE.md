# Kosh Adapter Gate

GrapheneDB has a runnable local Kosh adapter gate that validates the adapter boundary without requiring the external upstream repository/runtime.

## Commands

Windows:

```powershell
.\scripts\run_kosh_adapter_gate.ps1
```

POSIX:

```bash
scripts/run_kosh_adapter_gate.sh
```

## What it proves

- TSV interchange ingest through `KoshAdapter::ingest_tsv()`
- metadata preservation for Kosh-shaped records
- causal linking through the adapter
- causal bundle retrieval through the adapter
- reopen durability of adapter-ingested data

## Limitation

This is a local interchange gate, not live integration with an external KoshDB or LLM-Kosh runtime. The enterprise GA requirement for a live runtime path remains open.
