# One observation, one inspectable result

This exchange uses the **customer showcase C++ adapter**, backed by the actual engine. It is not an HTTP endpoint or a standalone public DB API. The adapter maps supplied targets, roles and verification into native storage and runtime calls; read `examples/customer_showcase/engine.cpp` when embedding it.

## Supply an observation

After the first seven incident observations, withdraw an invalid replay test. Target `0` indexes the release hypothesis; the adapter output uses native target IDs `1` and `2`.

```json
{
  "id": "bad-test-revoked",
  "content": "09:15 — The release replay used the wrong configuration; withdraw it.",
  "family": "test-audit",
  "target": 0,
  "kind": "revoke",
  "verified": true,
  "retire": "release-test"
}
```

The adapter's native call is:

```cpp
demo_add("bad-test-revoked",
         "09:15 — The release replay used the wrong configuration; withdraw it.",
         "test-audit", 0, "revoke", 1, "release-test");
```

`verified=1` is the application's supplied certificate decision. The engine does not establish source authenticity from this text. The prior event `release-test` must exist and be active; revocation retains its audit history.

## Inspect the response

The following is an **actual response excerpt** from the audit.2 native incident replay after this eighth observation. Omitted fields include the full ledger, runtime contract and events; this excerpt is not the whole receipt.

```json
{
  "status": "contested",
  "answer": 2,
  "targets": [
    {
      "id": 1,
      "content": "The new release caused checkout failures",
      "families": 1,
      "support": 0.9,
      "opposition": 0.95,
      "belief": 0.045
    },
    {
      "id": 2,
      "content": "Database connection exhaustion caused checkout failures",
      "families": 3,
      "support": 0.999,
      "opposition": 0,
      "belief": 0.999
    }
  ],
  "receipt": {
    "coreExecuted": true,
    "convergenceExecuted": true,
    "noSilentPromotion": true,
    "terminalCause": "dialectic_opposition_blocks_resolution"
  }
}
```

Target `2` leads with three independent supporting families. The state remains `contested`: material opposition in the evidence process blocks resolution. Keep the status, alternatives and terminal cause beside the selected answer. Support and belief numbers are not probabilities.

## Reproduce the complete exchange

After the [first example](DEVELOPER_QUICKSTART.md), run this optional replay from the examples directory:

```sh
python3 examples/customer_showcase/run.py
python3 -m json.tool reports/customer-showcase/incident.json
```

This runner replays all three synthetic scenarios. Inspect the penultimate state for the eighth incident observation; the final state verifies close/reopen. The full exported history contains the actual adapter responses.

For HTTP ingestion and runtime requests, use `python_http.py` and the [integration guide](AGENT_INTEGRATION.md). Do not send this adapter's JSON event to `/v1/extractions`: that endpoint has a different schema and does not expose this complete retirement flow.
