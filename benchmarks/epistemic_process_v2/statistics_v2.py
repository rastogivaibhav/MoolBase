#!/usr/bin/env python3
"""Frozen statistical primitives for EP-PROCESS-V2.

No third-party statistics library is required. All resampling is paired,
deterministic, and seeded. This module contains no MoolBase-specific outcome
logic.
"""
from __future__ import annotations

import math
import random
import statistics
from typing import Iterable, Mapping, Sequence

BOOTSTRAP_SEED = 20261006
BOOTSTRAP_RESAMPLES = 10000
ALPHA = 0.05


def _as_float_pair(
    control: Sequence[float | int | bool],
    treatment: Sequence[float | int | bool],
) -> tuple[list[float], list[float]]:
    if len(control) != len(treatment):
        raise ValueError("paired samples must have equal length")
    if not control:
        raise ValueError("paired samples must be non-empty")
    return [float(x) for x in control], [float(x) for x in treatment]


def mean(values: Sequence[float | int | bool]) -> float:
    if not values:
        raise ValueError("mean requires non-empty values")
    return float(statistics.fmean(float(x) for x in values))


def median(values: Sequence[float | int | bool]) -> float:
    if not values:
        raise ValueError("median requires non-empty values")
    return float(statistics.median(float(x) for x in values))


def exact_mcnemar(
    control: Sequence[bool],
    treatment: Sequence[bool],
) -> dict[str, float | int]:
    if len(control) != len(treatment):
        raise ValueError("McNemar inputs must have equal length")
    if not control:
        raise ValueError("McNemar inputs must be non-empty")

    b = sum(bool(c) and not bool(t) for c, t in zip(control, treatment))
    c = sum(not bool(c0) and bool(t) for c0, t in zip(control, treatment))
    discordant = b + c
    if discordant == 0:
        p_value = 1.0
    else:
        tail = sum(
            math.comb(discordant, k)
            for k in range(0, min(b, c) + 1)
        ) / (2 ** discordant)
        p_value = min(1.0, 2.0 * tail)

    return {
        "control_only_success": b,
        "treatment_only_success": c,
        "discordant_pairs": discordant,
        "p_value_two_sided_exact": p_value,
    }


def _quantile(sorted_values: Sequence[float], q: float) -> float:
    if not sorted_values:
        raise ValueError("quantile requires values")
    if q <= 0:
        return float(sorted_values[0])
    if q >= 1:
        return float(sorted_values[-1])
    pos = (len(sorted_values) - 1) * q
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return float(sorted_values[lo])
    weight = pos - lo
    return float(
        sorted_values[lo] * (1.0 - weight)
        + sorted_values[hi] * weight
    )


def paired_bootstrap_mean_delta(
    control: Sequence[float | int | bool],
    treatment: Sequence[float | int | bool],
    *,
    seed: int = BOOTSTRAP_SEED,
    resamples: int = BOOTSTRAP_RESAMPLES,
    alpha: float = ALPHA,
) -> dict[str, float | int]:
    c, t = _as_float_pair(control, treatment)
    if resamples <= 0:
        raise ValueError("resamples must be positive")
    rng = random.Random(seed)
    n = len(c)
    observed = mean(t) - mean(c)
    deltas: list[float] = []
    for _ in range(resamples):
        indexes = [rng.randrange(n) for _ in range(n)]
        c_mean = sum(c[i] for i in indexes) / n
        t_mean = sum(t[i] for i in indexes) / n
        deltas.append(t_mean - c_mean)
    deltas.sort()
    return {
        "n_pairs": n,
        "control_mean": mean(c),
        "treatment_mean": mean(t),
        "delta_treatment_minus_control": observed,
        "ci_low": _quantile(deltas, alpha / 2.0),
        "ci_high": _quantile(deltas, 1.0 - alpha / 2.0),
        "confidence_level": 1.0 - alpha,
        "bootstrap_seed": seed,
        "bootstrap_resamples": resamples,
    }


def paired_continuous_summary(
    control: Sequence[float | int],
    treatment: Sequence[float | int],
    *,
    seed: int = BOOTSTRAP_SEED,
    resamples: int = BOOTSTRAP_RESAMPLES,
) -> dict[str, object]:
    c, t = _as_float_pair(control, treatment)
    bootstrap = paired_bootstrap_mean_delta(
        c, t, seed=seed, resamples=resamples
    )
    differences = [tv - cv for cv, tv in zip(c, t)]
    return {
        "n_pairs": len(c),
        "control_mean": mean(c),
        "treatment_mean": mean(t),
        "control_median": median(c),
        "treatment_median": median(t),
        "mean_delta": bootstrap["delta_treatment_minus_control"],
        "median_paired_delta": median(differences),
        "mean_delta_ci_low": bootstrap["ci_low"],
        "mean_delta_ci_high": bootstrap["ci_high"],
        "bootstrap_seed": seed,
        "bootstrap_resamples": resamples,
    }


def paired_binary_summary(
    control: Sequence[bool],
    treatment: Sequence[bool],
    *,
    seed: int = BOOTSTRAP_SEED,
    resamples: int = BOOTSTRAP_RESAMPLES,
) -> dict[str, object]:
    bootstrap = paired_bootstrap_mean_delta(
        control, treatment, seed=seed, resamples=resamples
    )
    mcnemar = exact_mcnemar(control, treatment)
    return {
        **bootstrap,
        "exact_mcnemar": mcnemar,
    }


def holm_adjust(p_values: Mapping[str, float]) -> dict[str, float]:
    if not p_values:
        return {}
    for name, p in p_values.items():
        if not (0.0 <= float(p) <= 1.0):
            raise ValueError(f"invalid p-value for {name}: {p}")
    ordered = sorted(
        ((name, float(p)) for name, p in p_values.items()),
        key=lambda item: (item[1], item[0]),
    )
    m = len(ordered)
    adjusted: dict[str, float] = {}
    running = 0.0
    for index, (name, p) in enumerate(ordered):
        candidate = min(1.0, (m - index) * p)
        running = max(running, candidate)
        adjusted[name] = running
    return {name: adjusted[name] for name in p_values}


def ci_excludes_zero(summary: Mapping[str, object]) -> bool:
    low = float(summary["ci_low"])
    high = float(summary["ci_high"])
    return low > 0.0 or high < 0.0
