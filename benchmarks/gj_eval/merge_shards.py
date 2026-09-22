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

    expected = set(canonical)
    errors: list[str] = []
    if len(canonical) != len(expected):
        errors.append("world file contains duplicate world/timestep primary keys")

    merged: dict[tuple[str, int], dict[str, Any]] = {}
    shard_receipts: list[dict[str, Any]] = []
    transports: set[str] = set()
    requested_models: set[str] = set()
    resolved_models: set[str] = set()
    adapter_failures = 0

    for name in args.shards:
        path = Path(name)
        if not path.exists():
            errors.append(f"missing shard file: {path}")
            continue

        try:
            shard_rows = rows(path)
        except Exception as exc:
            errors.append(f"cannot parse shard {path}: {exc}")
            continue

        status_counts: dict[str, int] = {}
        shard_receipts.append(
            {
                "path": str(path),
                "rows": len(shard_rows),
                "sha256": sha256(path),
            }
        )

        for row in shard_rows:
            status = str(row.get("adapter_status"))
            status_counts[status] = status_counts.get(status, 0) + 1

            if row.get("system") != args.system:
                errors.append(
                    f"wrong system in {path}: {row.get('system')!r}"
                )
                continue

            try:
                key = (str(row["world_id"]), int(row["timestep"]))
            except Exception as exc:
                errors.append(f"invalid primary key in {path}: {exc}")
                continue

            if key not in expected:
                errors.append(f"unexpected primary key: {key}")
                continue
            if key in merged:
                errors.append(f"duplicate primary key: {key}")
                continue
            if row.get("adapter_status") != "ok":
                adapter_failures += 1
                errors.append(
                    f"adapter failure at {key}: {row.get('adapter_status')}"
                )
                merged[key] = row
                continue

            receipt = row.get("receipt")
            if args.system.startswith("jev"):
                if not isinstance(receipt, dict):
                    errors.append(f"missing Jev receipt at {key}")
                    merged[key] = row
                    continue
                transport = receipt.get("transport") or receipt.get("provider")
                requested_model = receipt.get("requested_model")
                resolved_model = receipt.get("model")
                if not transport or not requested_model:
                    errors.append(
                        f"missing Jev transport/model provenance at {key}"
                    )
                else:
                    transports.add(str(transport))
                    requested_models.add(str(requested_model))
                    if resolved_model:
                        resolved_models.add(str(resolved_model))

            merged[key] = row

        shard_receipts[-1]["adapter_status_counts"] = status_counts

    missing = [key for key in canonical if key not in merged]
    if missing:
        errors.append(
            f"missing {len(missing)} primary keys; first={missing[:5]}"
        )

    if args.system.startswith("jev"):
        if len(transports) != 1:
            errors.append(
                f"mixed or missing Jev transports in merged run: {sorted(transports)}"
            )
        if len(requested_models) != 1:
            errors.append(
                "mixed or missing requested Jev models in merged run: "
                f"{sorted(requested_models)}"
            )
        if len(resolved_models) > 1:
            errors.append(
                f"mixed resolved Jev models in merged run: {sorted(resolved_models)}"
            )

    output = Path(args.output)
    receipt_path = Path(args.receipt)
    receipt_path.parent.mkdir(parents=True, exist_ok=True)

    coverage_complete = not missing and len(merged) == len(canonical)
    receipt: dict[str, Any] = {
        "schema_version": 1,
        "system": args.system,
        "worlds_sha256": sha256(worlds_path),
        "expected_rows": len(canonical),
        "merged_rows": len(merged),
        "shards": shard_receipts,
        "coverage_complete": coverage_complete,
        "adapter_failures": adapter_failures,
        "transports": sorted(transports),
        "requested_models": sorted(requested_models),
        "resolved_models": sorted(resolved_models),
        "mixed_transport": len(transports) > 1,
        "mixed_requested_model": len(requested_models) > 1,
        "mixed_resolved_model": len(resolved_models) > 1,
        "errors": errors,
        "merge_accepted": not errors,
    }

    if errors:
        receipt["output_sha256"] = None
        receipt_path.write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps(receipt, sort_keys=True))
        return 1

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as handle:
        for key in canonical:
            handle.write(json.dumps(merged[key], sort_keys=True))
            handle.write("\n")

    receipt["output_sha256"] = sha256(output)
    receipt_path.write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
