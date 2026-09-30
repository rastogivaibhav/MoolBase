#!/usr/bin/env python3
from __future__ import annotations

from common_head_v2 import common_head


def meta(*rows):
    return {
        ref: {
            "family": family,
            "kind": kind,
            "bears_on": target,
            "depends_on": [],
            "revokes": [],
        }
        for ref, family, kind, target in rows
    }


def test_duplicate_family_neutral():
    metadata = meta(
        ("a1", "F_A", "support", "H1"),
        ("a2", "F_A", "support", "H1"),
        ("b1", "F_B", "support", "H2"),
    )
    result = common_head(["a1", "a2", "b1"], metadata)
    assert result["net_family_scores"] == {"H1": 1, "H2": 1}
    assert result["operative_hypothesis"] is None


def test_independent_families_accumulate():
    metadata = meta(
        ("a1", "F_A", "support", "H1"),
        ("a2", "F_B", "support", "H1"),
        ("b1", "F_C", "support", "H2"),
    )
    result = common_head(["a1", "a2", "b1"], metadata)
    assert result["operative_hypothesis"] == "H1"
    assert result["net_family_scores"] == {"H1": 2, "H2": 1}


def test_opposition_reduces_target_score():
    metadata = meta(
        ("a1", "F_A", "support", "H1"),
        ("a2", "F_B", "support", "H1"),
        ("x1", "F_X", "refute", "H1"),
        ("b1", "F_C", "support", "H2"),
    )
    result = common_head(["a1", "a2", "x1", "b1"], metadata)
    assert result["net_family_scores"] == {"H1": 1, "H2": 1}
    assert result["operative_hypothesis"] is None


def test_non_positive_does_not_select():
    metadata = meta(("x1", "F_X", "refute", "H1"))
    result = common_head(["x1"], metadata)
    assert result["operative_hypothesis"] is None


def main():
    test_duplicate_family_neutral()
    test_independent_families_accumulate()
    test_opposition_reduces_target_score()
    test_non_positive_does_not_select()
    print("cycle4_common_head_contracts=passed")


if __name__ == "__main__":
    main()
