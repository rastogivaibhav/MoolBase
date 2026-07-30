#!/usr/bin/env python3
"""Prepare a deterministic 2WikiMultiHopQA evidence-shape sample.

The preferred path uses the public Hugging Face datasets-server API so the
benchmark does not depend on executing a third-party dataset loading script.
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

DATASET = "framolfese/2WikiMultihopQA"
CONFIG = "default"
SPLIT = "validation"
EXPECTED_ROWS_FALLBACK = 12_576
API = "https://datasets-server.huggingface.co"


def get_json(url: str, attempts: int = 6) -> dict[str, Any]:
    last: Exception | None = None
    for attempt in range(attempts):
        try:
            request = urllib.request.Request(
                url,
                headers={"User-Agent": "GrapheneDB-2Wiki-Lyapunov/1.0"},
            )
            with urllib.request.urlopen(request, timeout=60) as response:
                return json.load(response)
        except Exception as error:  # pragma: no cover - network retry path
            last = error
            time.sleep(min(10.0, 1.5**attempt))
    raise RuntimeError(f"dataset request failed after {attempts} attempts: {last}")


def split_size() -> int:
    query = urllib.parse.urlencode({"dataset": DATASET})
    try:
        payload = get_json(f"{API}/size?{query}")
        for config in payload.get("size", {}).get("configs", []):
            if config.get("config") != CONFIG:
                continue
            for split in config.get("splits", []):
                if split.get("split") == SPLIT:
                    return int(split["num_rows"])
    except Exception as error:
        print(f"size_endpoint_warning={error}", file=sys.stderr)
    return EXPECTED_ROWS_FALLBACK


def fetch_rows(offset: int, length: int) -> list[dict[str, Any]]:
    query = urllib.parse.urlencode(
        {
            "dataset": DATASET,
            "config": CONFIG,
            "split": SPLIT,
            "offset": offset,
            "length": length,
        }
    )
    payload = get_json(f"{API}/rows?{query}")
    return [item["row"] for item in payload.get("rows", [])]


def normalise_record(record: dict[str, Any]) -> tuple[str, str, int, int, int, int] | None:
    metadata = record.get("metadata") if isinstance(record.get("metadata"), dict) else {}
    identifier = str(record.get("id") or record.get("_id") or metadata.get("id") or "")
    qtype = str(record.get("type") or metadata.get("type") or "unknown")
    supporting = record.get("supporting_facts") or metadata.get("supporting_facts") or {}
    context = record.get("context") or metadata.get("context") or {}
    evidences = record.get("evidences") or metadata.get("evidences") or []

    titles: list[str] = []
    sent_ids: list[Any] = []
    if isinstance(supporting, dict):
        titles = [str(value) for value in supporting.get("title", [])]
        sent_ids = list(supporting.get("sent_id", []))
    elif isinstance(supporting, list):
        for item in supporting:
            if isinstance(item, (list, tuple)) and len(item) >= 2:
                titles.append(str(item[0]))
                sent_ids.append(item[1])

    context_titles: list[str] = []
    if isinstance(context, dict):
        context_titles = [str(value) for value in context.get("title", [])]
    elif isinstance(context, list):
        for item in context:
            if isinstance(item, (list, tuple)) and item:
                context_titles.append(str(item[0]))

    hop_count = max(len(titles), len(sent_ids))
    if not identifier or hop_count < 2:
        return None
    unique_sources = len(set(titles)) or 1
    evidence_count = len(evidences) if isinstance(evidences, list) else hop_count
    distractor_count = max(0, len(set(context_titles)) - len(set(titles)))
    return (
        identifier.replace("\t", " ").replace(",", "_"),
        qtype.replace("\t", " ").replace(",", "_"),
        hop_count,
        unique_sources,
        evidence_count,
        distractor_count,
    )


def deterministic_offsets(total: int, limit: int, page_size: int, seed: int) -> list[int]:
    page_count = max(1, (total + page_size - 1) // page_size)
    pages_needed = min(page_count, max(1, (limit + page_size - 1) // page_size * 2))
    rng = random.Random(seed)
    pages = list(range(page_count))
    rng.shuffle(pages)
    selected = sorted(pages[:pages_needed])
    return [page * page_size for page in selected]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--limit", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=20260728)
    parser.add_argument("--page-size", type=int, default=100)
    args = parser.parse_args()
    if args.limit < 1 or args.limit > 5000:
        raise SystemExit("--limit must be within 1..5000")

    total = split_size()
    rows: list[tuple[str, str, int, int, int, int]] = []
    seen: set[str] = set()
    for offset in deterministic_offsets(total, args.limit, args.page_size, args.seed):
        for record in fetch_rows(offset, min(args.page_size, total - offset)):
            normalised = normalise_record(record)
            if normalised is None or normalised[0] in seen:
                continue
            seen.add(normalised[0])
            rows.append(normalised)
            if len(rows) >= args.limit:
                break
        if len(rows) >= args.limit:
            break

    if len(rows) < args.limit:
        raise RuntimeError(f"only prepared {len(rows)} of {args.limit} requested rows")

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as handle:
        handle.write("id\ttype\thops\tsource_count\tevidence_count\tdistractor_count\n")
        for row in rows:
            handle.write("\t".join(map(str, row)) + "\n")

    type_counts: dict[str, int] = {}
    for row in rows:
        type_counts[row[1]] = type_counts.get(row[1], 0) + 1
    manifest = {
        "dataset": DATASET,
        "split": SPLIT,
        "sample_size": len(rows),
        "seed": args.seed,
        "type_counts": type_counts,
        "temporal_fields_available": False,
    }
    output.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
