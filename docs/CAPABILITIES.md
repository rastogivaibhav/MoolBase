# Capabilities and maturity

MoolBase is a developer preview. “Works today” describes implemented behavior with bounded tests, not production certification.

| Area | Maturity | What you can rely on for evaluation |
|---|---|---|
| Embedded evidence storage | Works today | Native disk files, WAL/checkpoint, reopen and tested storage contracts |
| Correlated evidence, alternatives and revision | Works today | Typed inputs; supplied source families; opposition and explicit lifecycle operations through the native adapter |
| HypoKosh and DWM | Works today | Deterministic governed convergence, challenge, bounded reopen and inspectable runtime events |
| Receipts | Works today | Runtime receipts; reduced showcase exports. Applications persist receipts and prior reasoning state |
| Live browser Lab | Evaluation-only | Actual C++/WASM engine with synthetic fixtures and session-local files; refresh resets them |
| Compiled packages | Evaluation-only | audit.2 Linux x86_64 DB and examples; complete pinned source for other builds |
| Pilot HTTP API | Evaluation-only | Authenticated ingestion and runtime evaluation; incomplete lifecycle surface and fresh reasoning state per request |
| Shared hosted database or published MCP server | Not supplied | Embed the DB or operate your own pilot server/tool adapter |
| Natural-language evidence extraction and source authentication | Application responsibility | Supply targets, roles, source families and verification; independence is not discovered from prose |
| Production-scale performance and operational readiness | Not established | Evaluate persistence architecture, throughput, tooling and workload fit before production adoption |
| Learned decision/action layer | Research direction | No current trained policy or action-selection capability is claimed |
| Customer outcomes and superiority over other approaches | Not established | Current examples are synthetic; no quantified external workload advantage is claimed |

“Resolved” means the configured evidence policy was satisfied, not objective truth. Support scores are not probabilities. A selected answer may remain contested. MoolBase complements retrieval, graphs and event stores; simple retrieval usually needs a simpler tool.

Start with the [installation path](DEVELOPER_QUICKSTART.md), then inspect the [API exchange](API_EXAMPLE.md) and [validation evidence](https://moolbase.rasvai.com/VALIDATION.html).
