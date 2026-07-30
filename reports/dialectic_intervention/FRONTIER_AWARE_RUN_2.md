# Dialectic intervention benchmark — frontier-aware rerun

Total deterministic executions: **700**.

## Hard-family aggregate

| Policy | Final accuracy | Mean cycles | Mean visited states |
|---|---:|---:|---:|
| `broad_forced_3` | 83.3% | 3.00 | 34.00 |
| `no_cycle` | 0.0% | 0.00 | 2.67 |
| `targeted_current_stop` | 66.7% | 1.83 | 10.17 |
| `targeted_forced_3` | 100.0% | 3.00 | 16.00 |
| `targeted_frontier_aware` | 100.0% | 3.00 | 16.00 |

## Frozen diagnostic gates

- PASS — `frontier_aware_solves_all_hard_families`
- PASS — `current_stop_exposes_deep_chain_failure`
- PASS — `targeted_uses_less_search_than_broad`
- PASS — `broad_noise_trap_fails`
- PASS — `frontier_aware_noise_trap_passes`

## Claim boundary

This controlled benchmark isolates cycle, escape and stopping behaviour. It does not establish semantic truth, public-dataset generalisation or end-to-end superiority over external agent systems.
