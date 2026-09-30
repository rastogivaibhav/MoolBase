#!/usr/bin/env python3
from __future__ import annotations

from statistics_v2 import (
    exact_mcnemar,
    holm_adjust,
    paired_binary_summary,
    paired_bootstrap_mean_delta,
)


def main() -> None:
    extreme = exact_mcnemar(
        [False, False, False, False],
        [True, True, True, True],
    )
    assert extreme["control_only_success"] == 0
    assert extreme["treatment_only_success"] == 4
    assert extreme["discordant_pairs"] == 4
    assert abs(extreme["p_value_two_sided_exact"] - 0.125) < 1e-12

    tied = exact_mcnemar(
        [False, True, False, True],
        [True, False, False, True],
    )
    assert tied["control_only_success"] == 1
    assert tied["treatment_only_success"] == 1
    assert tied["p_value_two_sided_exact"] == 1.0

    a = paired_bootstrap_mean_delta(
        [0, 0, 1, 1],
        [1, 1, 1, 1],
        seed=123,
        resamples=1000,
    )
    b = paired_bootstrap_mean_delta(
        [0, 0, 1, 1],
        [1, 1, 1, 1],
        seed=123,
        resamples=1000,
    )
    assert a == b
    assert a["delta_treatment_minus_control"] == 0.5
    assert a["ci_low"] <= 0.5 <= a["ci_high"]

    adjusted = holm_adjust({"a": 0.01, "b": 0.04, "c": 0.03})
    assert abs(adjusted["a"] - 0.03) < 1e-12
    assert abs(adjusted["b"] - 0.06) < 1e-12
    assert abs(adjusted["c"] - 0.06) < 1e-12

    summary = paired_binary_summary(
        [False, False, True, True],
        [True, True, True, True],
        seed=44,
        resamples=1000,
    )
    assert summary["n_pairs"] == 4
    assert summary["exact_mcnemar"]["discordant_pairs"] == 2

    for bad in (-0.1, 1.1):
        try:
            holm_adjust({"bad": bad})
        except ValueError:
            pass
        else:
            raise AssertionError("Holm correction accepted invalid p-value")

    print("cycle6_statistics_contracts=passed")


if __name__ == "__main__":
    main()
