#!/usr/bin/env python3
"""Prepare deterministic structural metadata from public reasoning benchmarks.

The output intentionally excludes question/answer text. It preserves benchmark
identity, gold label, hop/evidence shape, source count, distractor count and
whether the task explicitly exercises temporal reasoning.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import random
import time
import urllib.parse
import urllib.request
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

USER_AGENT = "GrapheneDB-cross-dataset-epistemic/1.0"
BABI_TASKS = {
    "task2": "qa2_two-supporting-facts_test.txt",
    "task3": "qa3_three-supporting-facts_test.txt",
    "task9": "qa9_simple-negation_test.txt",
    "task14": "qa14_time-reasoning_test.txt",
    "task15": "qa15_basic-deduction_test.txt",
    "task17": "qa17_positional-reasoning_test.txt",
    "task19": "qa19_path-finding_test.txt",
}
BABI_BASE = (
    "https://raw.githubusercontent.com/seongsikpark/Q-MANN/"
    "ff84e8ca3f94b9f2f6dea3c03849efaa28a14569/"
    "MemN2N/dataset/tasks_1-20_v1-2/en-10k/"
)
HOTPOT_ROWS = "https://datasets-server.huggingface.co/rows"
FEVER_URLS = [
    "https://fever.ai/download/fever/paper_dev.jsonl",
    "https://s3-eu-west-1.amazonaws.com/fever.public/paper_dev.jsonl",
]


def get_bytes(url: str, attempts: int = 6) -> bytes:
    last: Exception | None = None
    for attempt in range(attempts):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request, timeout=90) as response:
                return response.read()
        except Exception as error:  # pragma: no cover - network retry path
            last = error
            time.sleep(min(12.0, 1.7**attempt))
    raise RuntimeError(f"download failed after {attempts} attempts: {url}: {last}")


def stable_sample(rows: list[tuple[Any, ...]], limit: int, seed: int) -> list[tuple[Any, ...]]:
    rng = random.Random(seed)
    rows = list(rows)
    rng.shuffle(rows)
    return rows[: min(limit, len(rows))]


def parse_babi(limit_per_task: int, seed: int, downloads: dict[str, str]) -> list[tuple[Any, ...]]:
    output: list[tuple[Any, ...]] = []
    for task_index, (task, filename) in enumerate(BABI_TASKS.items()):
        url = BABI_BASE + filename
        payload = get_bytes(url)
        downloads[url] = hashlib.sha256(payload).hexdigest()
        rows: list[tuple[Any, ...]] = []
        question_index = 0
        for raw in payload.decode("utf-8").splitlines():
            if "\t" not in raw:
                continue
            prefix, _answer, support_text = raw.split("\t", 2)
            line_id, _question = prefix.split(" ", 1)
            support_ids = [token for token in support_text.split() if token.isdigit()]
            if not support_ids:
                continue
            question_index += 1
            rows.append(
                (
                    "babi",
                    f"{task}:{question_index}:{line_id}",
                    "SUPPORT",
                    task,
                    len(support_ids),
                    1,
                    len(support_ids),
                    0,
                    1 if task == "task14" else 0,
                )
            )
        output.extend(stable_sample(rows, limit_per_task, seed + task_index))
    return output


def hotpot_page(offset: int, length: int) -> list[dict[str, Any]]:
    query = urllib.parse.urlencode(
        {
            "dataset": "hotpotqa/hotpot_qa",
            "config": "distractor",
            "split": "validation",
            "offset": offset,
            "length": length,
        }
    )
    return json.loads(get_bytes(f"{HOTPOT_ROWS}?{query}"))["rows"]


def parse_hotpot(limit: int, seed: int, downloads: dict[str, str]) -> list[tuple[Any, ...]]:
    rows: list[tuple[Any, ...]] = []
    offset = 0
    while len(rows) < limit * 2 and offset < 7405:
        page = hotpot_page(offset, min(100, 7405 - offset))
        downloads[f"hotpot-page:{offset}"] = hashlib.sha256(
            json.dumps(page, sort_keys=True).encode("utf-8")
        ).hexdigest()
        for wrapper in page:
            row = wrapper.get("row", wrapper)
            supporting = row.get("supporting_facts", {})
            context = row.get("context", {})
            titles = [str(value) for value in supporting.get("title", [])]
            sent_ids = list(supporting.get("sent_id", []))
            context_titles = [str(value) for value in context.get("title", [])]
            hops = max(len(titles), len(sent_ids))
            if hops < 1:
                continue
            rows.append(
                (
                    "hotpotqa",
                    str(row.get("id") or row.get("_id")),
                    "SUPPORT",
                    str(row.get("type", "unknown")),
                    hops,
                    max(1, len(set(titles))),
                    hops,
                    max(0, len(set(context_titles)) - len(set(titles))),
                    0,
                )
            )
        offset += len(page)
        if not page:
            break
    if len(rows) < limit:
        raise RuntimeError(f"HotpotQA yielded only {len(rows)} rows for requested {limit}")
    return stable_sample(rows, limit, seed)


def first_complete_evidence_set(evidence: Any) -> list[list[Any]]:
    if not isinstance(evidence, list):
        return []
    candidates: list[list[list[Any]]] = []
    for evidence_set in evidence:
        if not isinstance(evidence_set, list):
            continue
        complete = [
            item
            for item in evidence_set
            if isinstance(item, list) and len(item) >= 4 and item[2] is not None and item[3] is not None
        ]
        if complete:
            candidates.append(complete)
    return min(candidates, key=len) if candidates else []


def parse_fever(limit_per_label: int, seed: int, downloads: dict[str, str]) -> list[tuple[Any, ...]]:
    payload: bytes | None = None
    source_url = ""
    errors: list[str] = []
    for url in FEVER_URLS:
        try:
            payload = get_bytes(url)
            source_url = url
            break
        except Exception as error:  # pragma: no cover - fallback path
            errors.append(str(error))
    if payload is None:
        raise RuntimeError("all FEVER downloads failed: " + " | ".join(errors))
    downloads[source_url] = hashlib.sha256(payload).hexdigest()
    grouped: dict[str, list[tuple[Any, ...]]] = defaultdict(list)
    for raw in payload.decode("utf-8").splitlines():
        if not raw.strip():
            continue
        row = json.loads(raw)
        label = str(row.get("label", ""))
        if label == "SUPPORTS":
            category = "SUPPORT"
        elif label == "REFUTES":
            category = "REFUTE"
        elif label == "NOT ENOUGH INFO":
            category = "NEI"
        else:
            continue
        evidence_set = first_complete_evidence_set(row.get("evidence"))
        pages = {str(item[2]) for item in evidence_set if item[2] is not None}
        hops = len(evidence_set) if evidence_set else 1
        grouped[label].append(
            (
                "fever",
                str(row["id"]),
                category,
                label.replace(" ", "_"),
                hops,
                len(pages),
                len(evidence_set),
                0,
                0,
            )
        )
    output: list[tuple[Any, ...]] = []
    for index, label in enumerate(("SUPPORTS", "REFUTES", "NOT ENOUGH INFO")):
        selected = stable_sample(grouped[label], limit_per_label, seed + index)
        if len(selected) < limit_per_label:
            raise RuntimeError(f"FEVER {label} yielded {len(selected)} rows")
        output.extend(selected)
    return output


def write_rows(path: Path, rows: Iterable[tuple[Any, ...]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(
            "dataset\tid\tcategory\tsubtype\thops\tsource_count\t"
            "evidence_count\tdistractor_count\ttemporal\n"
        )
        for row in rows:
            handle.write("\t".join(map(str, row)) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--babi-per-task", type=int, default=200)
    parser.add_argument("--hotpot", type=int, default=500)
    parser.add_argument("--fever-per-label", type=int, default=200)
    parser.add_argument("--seed", type=int, default=20260729)
    args = parser.parse_args()

    downloads: dict[str, str] = {}
    rows = []
    rows.extend(parse_babi(args.babi_per_task, args.seed, downloads))
    rows.extend(parse_hotpot(args.hotpot, args.seed + 100, downloads))
    rows.extend(parse_fever(args.fever_per_label, args.seed + 200, downloads))
    rows.sort(key=lambda row: (str(row[0]), str(row[1]), str(row[2])))

    output = Path(args.output)
    write_rows(output, rows)
    counts: dict[str, int] = defaultdict(int)
    categories: dict[str, int] = defaultdict(int)
    for row in rows:
        counts[str(row[0])] += 1
        categories[f"{row[0]}:{row[2]}"] += 1
    manifest = {
        "seed": args.seed,
        "examples": len(rows),
        "datasets": dict(sorted(counts.items())),
        "categories": dict(sorted(categories.items())),
        "downloads_sha256": dict(sorted(downloads.items())),
        "claim_boundary": "structural epistemic diagnostic; not answer accuracy",
    }
    output.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
