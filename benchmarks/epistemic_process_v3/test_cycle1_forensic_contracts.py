#!/usr/bin/env python3
"""V3 Cycle-1 forensic contracts.

These tests reproduce V2 mechanisms only. They do not change or repair
production behavior.
"""
from __future__ import annotations

import sys
from pathlib import Path

V2 = Path(__file__).resolve().parents[1] / "epistemic_process_v2"
sys.path.insert(0, str(V2))

from common_head_v2 import common_head  # noqa: E402


def insufficient_replacement_contract() -> None:
    metadata = {
        "e1": {"family": "A", "kind": "support", "bears_on": "H1"},
        "e2": {"family": "B", "kind": "support", "bears_on": "H1"},
        "e3": {"family": "R", "kind": "refute", "bears_on": "H1"},
        "e4": {"family": "C", "kind": "support", "bears_on": "H2"},
    }

    # Stateless C0 sees only the current observation and therefore selects H2.
    c0 = common_head(["e4"], metadata)
    assert c0["operative_hypothesis"] == "H2"
    assert c0["net_family_scores"] == {"H1": 0, "H2": 1}

    # Persistent C1 retains prior support plus the refutation. Refutation is
    # opposition, not revocation, so H1 retains +2 support and -1 opposition.
    # H2 has +1 support. The common head sees a 1:1 tie and emits open.
    c1 = common_head(["e1", "e2", "e3", "e4"], metadata)
    assert c1["operative_hypothesis"] is None
    assert c1["status"] == "open"
    assert c1["net_family_scores"] == {"H1": 1, "H2": 1}

    # If the two prior H1 supports are explicitly removed from the active
    # evidence set, the same head selects H2. This distinguishes refutation
    # from invalidation/revocation.
    revoked = common_head(["e3", "e4"], metadata)
    assert revoked["operative_hypothesis"] == "H2"
    assert revoked["net_family_scores"] == {"H1": -1, "H2": 1}


def label_symmetry_of_common_head() -> None:
    metadata = {
        "a": {"family": "FA", "kind": "support", "bears_on": "H1"},
        "b": {"family": "FB", "kind": "support", "bears_on": "H2"},
    }
    original = common_head(["a", "b"], metadata)
    assert original["operative_hypothesis"] is None

    swapped = {
        ref: {
            **row,
            "bears_on": "H2" if row["bears_on"] == "H1" else "H1",
        }
        for ref, row in metadata.items()
    }
    mirrored = common_head(["a", "b"], swapped)
    assert mirrored["operative_hypothesis"] is None


def main() -> None:
    insufficient_replacement_contract()
    label_symmetry_of_common_head()
    print(
        "cycle1_forensic_contracts=passed "
        "refutation_is_not_revocation=true common_head_tie_abstains=true"
    )


if __name__ == "__main__":
    main()
