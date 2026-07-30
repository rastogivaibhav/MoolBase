#!/usr/bin/env python3
"""Fail when the pilot server exposes a path absent from OpenAPI."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def read_server_source(path: Path) -> str:
    """Read the server translation unit and local generated include fragments."""
    source = path.read_text(encoding="utf-8")
    fragments = []
    for include in re.findall(r'^#include\s+"(graphenedb_server_part_[^"]+\.inc)"\s*$', source, flags=re.MULTILINE):
        fragments.append((path.parent / include).read_text(encoding="utf-8"))
    return source + "\n" + "\n".join(fragments)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_openapi_surface.py <server.cpp> <openapi.yaml>")

    server = read_server_source(Path(sys.argv[1]))
    openapi_path = Path(sys.argv[2])
    openapi = openapi_path.read_text(encoding="utf-8")
    supplement = openapi_path.with_name("openapi-v1-runtime.yaml")
    if supplement.exists():
        openapi += "\n" + supplement.read_text(encoding="utf-8")

    implemented = set(re.findall(r'path\s*==\s*"(/v1/[^"]+)"', server))
    implemented.update(re.findall(r'path\.rfind\("(/v1/[^"]+)"', server))
    implemented.discard("/v1/nodes/")
    implemented.add("/v1/nodes/{nodeId}")

    documented = set(re.findall(r"^  (/v1/[^:]+):\s*$", openapi, flags=re.MULTILINE))

    missing = sorted(implemented - documented)
    extra = sorted(documented - implemented)
    if missing or extra:
        print(f"missing_from_openapi={missing}")
        print(f"not_implemented={extra}")
        return 1

    lattice_hops = re.search(
        r"/v1/search/lattice:.*?name: hops.*?maximum:\s*(\d+)",
        openapi,
        flags=re.DOTALL,
    )
    if not lattice_hops or lattice_hops.group(1) != "16":
        print("lattice_hops_contract_must_equal_16")
        return 1

    print(f"openapi_surface_paths={len(documented)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
