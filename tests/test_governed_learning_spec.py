#!/usr/bin/env python3
"""Positive and negative contracts for the governed-learning spec validator."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def run(validator: pathlib.Path, spec: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(validator), str(spec)],
        check=False,
        capture_output=True,
        text=True,
    )


def main() -> int:
    if len(sys.argv) != 3:
        return 2
    validator = pathlib.Path(sys.argv[1]).resolve()
    spec = pathlib.Path(sys.argv[2]).resolve()

    valid = run(validator, spec)
    assert valid.returncode == 0, valid.stderr
    assert "requirements=22" in valid.stdout

    text = spec.read_text(encoding="utf-8")
    broken_text = text.replace("## Policy learning", "## Missing policy section", 1)
    with tempfile.TemporaryDirectory(prefix="graphenedb-learning-spec-") as temp:
        broken = pathlib.Path(temp) / "broken.md"
        broken.write_text(broken_text, encoding="utf-8")
        invalid = run(validator, broken)
        assert invalid.returncode == 1
        assert "missing section: ## Policy learning" in invalid.stderr

    print("governed_learning_spec_contract_passed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
