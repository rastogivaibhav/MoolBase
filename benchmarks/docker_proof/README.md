# Docker proof harness

This harness turns the current GrapheneDB source tree into a reproducible local validation appliance. It exists to let a developer, reviewer or paper reader run the same bounded evidence gates without installing a C++ toolchain on the host.

## Modes

| Mode | What it runs | Network |
|---|---|---|
| `api-smoke` | Starts the packaged server through Compose, imports a five-node evidence graph and verifies deterministic repeated reasoning | local Compose network only |
| `smoke` | Critical CTest contracts, demo, live server contract, 700-run intervention benchmark and offline cross-dataset structural gate | none required |
| `full` | Complete alpha release gate plus server-enabled runtime contract | none required |
| `security` | ASAN/UBSAN/TSAN and 10,000 libFuzzer executions | none required |
| `public` | Frozen 2,500-record bAbI/HotpotQA/FEVER structural benchmark | external download required |

Every completed mode writes a content-addressed evidence directory and `.tar.gz` archive under `docker-evidence/`.

## Frozen claims

The controlled intervention thresholds in `expected_gates.json` reflect the committed experiment:

- 700 deterministic executions;
- frontier-aware targeted recovery: 100% hard-family accuracy;
- broad forced retrieval: 83.3%;
- targeted mean visited states: 16;
- broad mean visited states: 34.

These values validate the controller mechanism in the frozen synthetic families. They are not end-to-end public benchmark answer scores.
