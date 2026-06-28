# GrapheneDB GA readiness scorecard

## Summary

| Area | Score | Status |
|---|---:|---|
| Product positioning | 8/10 | Clear as embedded causal-memory DB. |
| Build/package hygiene | 8/10 | Clean repo, scripts, CI, CMake presets. |
| Core API | 7/10 | Useful v1 API; batch transaction API still missing. |
| Durability | 7/10 | WAL/checkpoint/replay present; deeper crash matrix needed. |
| Safety | 7/10 | Validation/sanitizer gates present; longer fuzzing needed. |
| Performance | 6/10 | 1M storage stress evidence; larger realistic graph/vector tests needed. |
| Operability | 6/10 | CLI, inspect, backup, compact; no deep repair tool yet. |
| Integration | 5/10 | Kosh adapter skeleton and TSV gate; live repo integration pending. |
| Documentation | 8/10 | Strong RC docs, acceptance, handoff, examples. |
| Enterprise GA | 5/10 | Controlled pilot candidate, not enterprise GA. |

## Current overall readiness

**Controlled pilot:** 8/10  
**Public developer preview:** 7/10  
**Enterprise GA:** 5/10

## Must-fix before enterprise GA

1. Run a true 24-hour soak on a persistent machine.
2. Run multi-hour coverage-guided fuzzing and preserve corpus/crash artifacts.
3. Run 1M-node test with realistic dimensions, metadata, and causal graph density.
4. Integrate against the real KoshDB/LLM-Kosh runtime.
5. Add batch transaction API.
6. Add persistent secondary metadata indexes or document rebuild cost clearly.
7. Add WAL retention/rotation policy with cleanup controls.
8. Add manifest checksum and stronger storage file verification.
9. Add disk-pressure and power-loss style crash simulation.
10. Add release versioning, semantic version tags, and final license.

## Should-fix before public developer preview

- Improve CLI JSON output mode.
- Add Python binding or C ABI if the target user is not C++.
- Add benchmark comparison against vector-only baseline using the same dataset.
- Add richer examples for contradiction and supersession.
- Add explicit storage format versioning docs.

## What should remain out of v1

- Distributed clustering.
- SQL parser.
- Cloud SaaS control plane.
- Neo4j/Qdrant replacement claims.
- Multi-tenant authentication server.

## Recommended release label

`GrapheneDB v1.0-rc3 — embedded causal-memory DB for controlled pilots`
