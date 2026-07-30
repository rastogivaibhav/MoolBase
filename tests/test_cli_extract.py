#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def run_capture(command: list[str]) -> str:
    proc = subprocess.run(command, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise SystemExit(proc.stdout + proc.stderr)
    return proc.stdout + proc.stderr


def main() -> int:
    if len(sys.argv) < 2:
        raise SystemExit("usage: test_cli_extract.py <graphenedb_cli>")
    cli = Path(sys.argv[1])
    base = Path(tempfile.gettempdir()) / "graphenedb_cli_extract_test"
    if base.exists():
        subprocess.run(["cmd", "/c", "rmdir", "/s", "/q", str(base)], capture_output=True, text=True)
    base.mkdir(parents=True, exist_ok=True)
    db = base / "db"
    tsv = base / "extraction.tsv"
    sig = (1 << 3) | (1 << (16 + 4))
    tsv.write_text(
        (
            f"root-a\troot extracted claim\t0.1,0.2,0.3\t{sig}\t9\troot\n"
            f"symptom-a\tsymptom extracted claim\t0.11,0.21,0.29\t{sig}\t9\tsymptom\n"
            f"impact-a\timpact extracted claim\t0.12,0.22,0.28\t{sig}\t9\timpact\n"
        ),
        encoding="utf-8",
    )

    first = run_capture([str(cli), "extract-tsv", str(db), "3", "research-pack", str(tsv)])
    assert "inserted_nodes=3" in first
    assert "existing_nodes=0" in first
    assert "inserted_edges=2" in first

    second = run_capture([str(cli), "extract-tsv", str(db), "3", "research-pack", str(tsv)])
    assert "inserted_nodes=0" in second
    assert "existing_nodes=3" in second
    assert "inserted_edges=0" in second

    inspect = run_capture([str(cli), "inspect", str(db), "3"])
    assert "nodes_visible=3" in inspect
    assert "edges_visible=2" in inspect
    assert "lattice_nodes=3" in inspect

    print("graphenedb_cli_extract_tests_passed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
