#!/usr/bin/env python3
"""Strictly merge deterministic execution shards without changing GJ-Eval semantics."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


def rows(path: Path) -> list[dict[str, Any]]:
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--shards", nargs="+", required=True)
    parser.add_argument("--system", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--receipt", required=True)
    args = parser.parse_args()

    worlds_path = Path(args.worlds)
    worlds = rows(worlds_path)
    canonical: list[tuple[str, int]] = []
    for world in worlds:
        for step in world["timeline"]:
            canonical.append((str(world["world_id"]), int(step["timestep"])))

    if len(canonical) != len(set(canonical)):
        raise SystemExit("world file contains duplicate world/timestep primary keys")

    expected = set(canonical)
    merged: dict[tuple[str, int], dict[str, Any]] = {}
    shard_receipts: list[dict[str, Any]] = []
    transports: set[str] = set()
    requested_models: set[str] = set()
    resolved_models: set[str] = set()

    for name in args.shards:
        path = Path(name)
        shard_rows = rows(path)
        shard_receipts.append(
            {
                "path": str(path),
                "rows": len(shard_rows),
                "sha256": sha256(path),
            }
        )
        for row in shard_rows:
            if row.get("system") != args.system:
                raise SystemExit(
                    f"wrong system in {path}: {row.get('system')!r}"
                )

            key = (str(row["world_id"]), int(row["timestep"]))
            if key not in expected:
                raise SystemExit(f"unexpected primary key: {key}")
            if key in merged:
                raise SystemExit(f"duplicate primary key: {key}")
            if row.get("adapter_status") != "ok":
                raise SystemExit(
                    f"adapter failure at {key}: {row.get('adapter_status')}"
                )

            receipt = row.get("receipt")
            if args.system.startswith("jev"):
                if not isinstance(receipt, dict):
                    raise SystemExit(f"missing Jev receipt at {key}")
                transport = receipt.get("transport") or receipt.get("provider")
                requested_model = receipt.get("requested_model")
                resolved_model = receipt.get("model")
                if not transport or not requested_model:
                    raise SystemExit(
                        f"missing Jev transport/model provenance at {key}"
                    )
                transports.add(str(transport))
                requested_models.add(str(requested_model))
                if resolved_model:
                    resolved_models.add(str(resolved_model))

            merged[key] = row

    missing = [key for key in canonical if key not in merged]
    if missing:
        raise SystemExit(
            f"missing {len(missing)} primary keys; first={missing[:5]}"
        )

    if args.system.startswith("jev"):
        if len(transports) != 1:
            raise SystemExit(f"mixed Jev transports in merged run: {sorted(transports)}")
        if len(requested_models) != 1:
            raise SystemExit(
                f"mixed requested Jev models in merged run: {sorted(requested_models)}"
            )
        if len(resolved_models) > 1:
            raise SystemExit(
                f"mixed resolved Jev models in merged run: {sorted(resolved_models)}"
            )

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as handle:
        for key in canonical:
            handle.write(json.dumps(merged[key], sort_keys=True))
            handle.write("\n")

    receipt = {
        "schema_version": 1,
        "system": args.system,
        "worlds_sha256": sha256(worlds_path),
        "expected_rows": len(canonical),
        "merged_rows": len(merged),
        "output_sha256": sha256(output),
        "shards": shard_receipts,
        "coverage_complete": True,
        "adapter_failures": 0,
        "transports": sorted(transports),
        "requested_models": sorted(requested_models),
        "resolved_models": sorted(resolved_models),
        "mixed_transport": len(transports) > 1,
        "mixed_requested_model": len(requested_models) > 1,
        "mixed_resolved_model": len(resolved_models) > 1,
    }

    receipt_path = Path(args.receipt)
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
