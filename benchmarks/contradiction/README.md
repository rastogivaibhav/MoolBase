# Contradiction and stale-evidence benchmark

Seeded, deterministic, no model or network. Drives the real native adapter
(`build/moolbase_customer_showcase`) and compares it with three simple baselines.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target moolbase_customer_showcase -j2
python3 benchmarks/contradiction/run_contradiction_bench.py --per-kind 200
```

Scenarios: `update_chain`, `echo_flood`, `revoked_majority`, `unverified_flood`, `true_tie`.
A prediction is correct if it matches the intended answer, or abstains on `true_tie`.
`resolved` and `provisionally_resolved` count as a commitment; anything else is an abstention.
The report also lists wrong answers that carried the high-confidence `resolved` status.

## Limits (important)

- Scenarios and ground truth are self-authored. This checks that MoolBase implements its documented
  evidence policy; it is not an external benchmark and does not test natural-language extraction.
- Graphiti, Letta and BeliefMem are not run (they need LLM keys and network access).
- `verified_family_rule` re-implements the policy under test in ten lines, so it scores 100% by
  construction. It shows what a plain rule can do, not what another product does.

## Reference result (seed 20261122, 200 per scenario, 1000 streams)

| Scenario | MoolBase | last-write-wins | count-majority |
|---|---|---|---|
| update_chain | 100% | 100% | 100% |
| echo_flood | 100% | 33% | 0% |
| revoked_majority | 100% | 62% | 100% |
| unverified_flood | **0%** | 35% | 0% |
| true_tie | 100% | 0% | 100% |

On `unverified_flood` the adapter counts unverified source families as support, so five unverified
families outrank two verified ones. Those answers are capped at `provisionally_resolved` and carry an
"externally unverified" caveat; no wrong answer reached `resolved` in any run.
