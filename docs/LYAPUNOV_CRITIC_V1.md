# Complete Discrete Lyapunov Critic v1

## Purpose

The critic is a stability controller for the bounded HypoKosh reasoning loop. It is not a truth oracle and it does not treat a fluent or internally coherent answer as proof.

The original paper-defined factors are retained:

- temporal consistency;
- path diversity;
- independent-path degeneracy;
- provenance quality;
- contradiction pressure;
- premature pattern lock;
- missing evidence.

The earlier `StabilityCriticV0` evaluated one bundle with a weighted score. The complete critic adds an explicit error state, a quadratic Lyapunov candidate, transition drift, sufficient-decrease checks, finite-trajectory certificates, oscillation detection, and repeated-state limit-cycle detection.

## Error state

For a reasoning bundle `B`, the critic constructs a normalised error vector:

```text
x(B) = [
  temporal_deficit,
  diversity_deficit,
  degeneracy_deficit,
  provenance_deficit,
  contradiction_excess,
  pattern_lock_excess,
  missing_evidence_excess
]
```

Every coordinate is bounded in `[0, 1]`. The origin represents the configured acceptable epistemic goal set. It does not represent universal or metaphysical truth.

Lower-bound metrics use:

```text
deficit(actual, target) = clamp((target - actual) / target, 0, 1)
```

Upper-bound metrics use:

```text
excess(actual, maximum) =
  0                                           when actual <= maximum
  clamp((actual - maximum)/(1 - maximum),0,1) otherwise
```

Empirical, balanced, and theoretical modes adjust the target set. Empirical mode requires stronger time and provenance discipline. Theoretical mode requires more diversity while allowing a larger explicitly-labelled missing-evidence region.

## Lyapunov candidate

The candidate energy is:

```text
V(B) = (sum_i w_i * x_i(B)^2) / sum_i w_i
```

All configured weights must be finite and strictly positive. Therefore, in the error coordinates:

```text
(lambda_min / sum w) * ||x||^2 <= V(x)
V(x) <= (lambda_max / sum w) * ||x||^2
```

The runtime returns the lower and upper quadratic coefficients as part of the certificate. This establishes positive-definite quadratic bounds relative to the configured goal set.

## Transition analysis

For reasoning cycle `k`:

```text
Delta V_k = V(B_{k+1}) - V(B_k)
```

Sufficient decrease is observed when:

```text
Delta V_k <= -alpha * ||x(B_k)||^2
```

The critic classifies each transition as:

- `equilibrium`;
- `descending`;
- `marginal`;
- `diverging`;
- `oscillating`;
- `limit_cycle`;
- `insufficient_history`.

A repeated non-equilibrium immutable bundle hash after an intervening state is treated as a finite-state limit-cycle signal. Alternating positive and negative drift within the configured window is treated as oscillation.

## Certificate

The finite-trajectory certificate records:

- positive weights;
- bounded error state;
- non-negative finite energy;
- valid quadratic lower and upper bounds;
- monotonic non-increase across observed transitions;
- strict or sufficient decrease outside the goal set;
- initial and final energy;
- mean and worst observed contraction ratio;
- maximum energy increase;
- practical-goal arrival;
- equilibrium dwell;
- observed convergence;
- oscillation and limit-cycle findings;
- explicit violations.

`practical_stability_observed` means the observed final state is inside the configured energy set with a valid energy construction and no detected limit cycle. `convergence_observed` additionally requires dwell and monotonicity over the finite recorded trajectory.

## Formal boundary

This implementation provides a mathematically explicit Lyapunov candidate and a reproducible certificate over the observed bounded reasoning trajectory. It does **not** prove global asymptotic stability for every possible future GrapheneDB state or every external discovery action. Such a proof would require a fully specified transition function and invariance arguments over the complete state space.

The accurate claim is:

> The system now performs discrete Lyapunov analysis over its finite reasoning trajectory and can certify or reject observed practical stability under configured assumptions.
