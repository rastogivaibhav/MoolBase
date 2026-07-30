#!/usr/bin/env python3
"""Exercise atomic extraction and dialectic retrieval through the pilot API.

The input is the public icco/postmortems corpus. Categories are supplied
annotations, so this evaluates graph ingestion and retrieval rather than
autonomous root-cause discovery.
"""

from __future__ import annotations

import json
import os
import pathlib
import signal
import socket
import subprocess
import sys
import tempfile
import time
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLIENT_DIR = ROOT / "clients" / "python"
sys.path.insert(0, str(CLIENT_DIR))
from graphenedb_client import GrapheneDBClient  # noqa: E402

API_KEY = "real-postmortem-api-eval"


def scalar(value: str) -> str:
    value = value.strip()
    if value.startswith('"') and value.endswith('"'):
        try:
            return str(json.loads(value))
        except json.JSONDecodeError:
            return value[1:-1]
    if value.startswith("'") and value.endswith("'"):
        return value[1:-1].replace("''", "'")
    return value


def load_records(directory: pathlib.Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for path in sorted(directory.glob("*.md")):
        lines = path.read_text(encoding="utf-8").splitlines()
        if not lines or lines[0].strip() != "---":
            continue
        fields: dict[str, str] = {}
        categories: list[str] = []
        active_list = ""
        for line in lines[1:]:
            if line.strip() == "---":
                break
            if line.startswith("- "):
                if active_list == "categories":
                    categories.append(scalar(line[2:]))
                continue
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            key = key.strip()
            active_list = key if not value.strip() else ""
            if value.strip():
                fields[key] = scalar(value)
        uuid = fields.get("uuid", "")
        title = fields.get("title", "")
        if uuid and title and categories:
            records.append(
                {
                    "uuid": uuid,
                    "title": title,
                    "url": fields.get("url", str(path)),
                    "start_time": fields.get("start_time", ""),
                    "company": fields.get("company", ""),
                    "product": fields.get("product", ""),
                    "categories": sorted(set(categories)),
                }
            )
    records.sort(key=lambda record: record["uuid"])
    return records


def fnv64(value: str) -> int:
    result = 1469598103934665603
    for byte in value.encode("utf-8"):
        result ^= byte
        result = (result * 1099511628211) & ((1 << 64) - 1)
    return result


def signature_for_record(uuid: str) -> int:
    hashed = fnv64(uuid)
    service = hashed & 15
    symptom = (hashed >> 8) & 15
    return (1 << service) | (1 << (16 + symptom))


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def wait_ready(client: GrapheneDBClient, process: subprocess.Popen[Any]) -> None:
    for _ in range(400):
        if process.poll() is not None:
            raise RuntimeError(f"server exited during startup with {process.returncode}")
        try:
            if client.ready().status == 200:
                return
        except Exception:
            pass
        time.sleep(0.025)
    raise RuntimeError("server did not become ready")


def start_server(
    binary: pathlib.Path, database: pathlib.Path, port: int, log
) -> tuple[subprocess.Popen[Any], GrapheneDBClient]:
    environment = dict(os.environ)
    environment["GRAPHENEDB_API_KEY"] = API_KEY
    command = [
        str(binary),
        str(database),
        "64",
        str(port),
        "--physical-lattice-primary",
        "--physical-lattice-radius",
        "64",
        "--expected-max-nodes",
        "12481",
        "--workers",
        "8",
        "--queue-capacity",
        "256",
        "--rate-limit-rps",
        "10000",
        "--rate-limit-burst",
        "10000",
        "--max-request-bytes",
        str(8 * 1024 * 1024),
        "--max-bulk-nodes",
        "1000",
    ]
    process = subprocess.Popen(
        command,
        stdout=subprocess.DEVNULL,
        stderr=log,
        env=environment,
        text=True,
    )
    client = GrapheneDBClient(
        f"http://127.0.0.1:{port}", api_key=API_KEY, timeout=30
    )
    wait_ready(client, process)
    return process, client


def stop_server(process: subprocess.Popen[Any]) -> None:
    process.send_signal(signal.SIGTERM)
    process.wait(timeout=30)
    if process.returncode != 0:
        raise RuntimeError(f"server shutdown returned {process.returncode}")


def percentile(values: list[float], percentile_value: int) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(
        len(ordered) - 1,
        int((percentile_value / 100.0) * (len(ordered) - 1)),
    )
    return ordered[index]


def build_extraction(records: list[dict[str, Any]]) -> dict[str, Any]:
    categories = sorted(
        {category for record in records for category in record["categories"]}
    )
    nodes: list[dict[str, Any]] = [
        {
            "external_id": f"category/{category}",
            "content": f"annotated incident category: {category}",
            "role": "root",
            "metadata": {"dataset": "icco/postmortems", "annotation": "category"},
        }
        for category in categories
    ]
    relations: list[dict[str, Any]] = []
    for index, record in enumerate(records):
        signature = signature_for_record(record["uuid"])
        nodes.append(
            {
                "external_id": f"incident/{record['uuid']}",
                "content": record["title"],
                "signature": signature,
                "incident": index + 1,
                "role": "symptom",
                "metadata": {
                    "dataset": "icco/postmortems",
                    "uuid": record["uuid"],
                    "source": record["url"],
                    "company": record["company"],
                    "product": record["product"],
                },
            }
        )
        for category in record["categories"]:
            relation: dict[str, Any] = {
                "from_external_id": f"category/{category}",
                "to_external_id": f"incident/{record['uuid']}",
                "origin": "observed",
                "role": "causal",
                "confidence": 0.95,
                "evidence_id": record["url"],
                "evidence_text": "curated incident category annotation",
            }
            if record["start_time"]:
                relation["metadata"] = {"observed_at": record["start_time"]}
            relations.append(relation)
    return {
        "schema_version": 1,
        "source_id": "icco-postmortems-pinned-corpus",
        "source_uri": "https://github.com/icco/postmortems",
        "extraction_run_id": "graphenedb-real-api-eval",
        "nodes": nodes,
        "relations": relations,
    }


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: server_real_postmortem_api_eval.py "
            "<graphenedb_server> <postmortem-data-dir>",
            file=sys.stderr,
        )
        return 2
    binary = pathlib.Path(sys.argv[1]).resolve()
    corpus = pathlib.Path(sys.argv[2]).resolve()
    records = load_records(corpus)
    if not records:
        raise RuntimeError("no usable postmortem records found")
    extraction = build_extraction(records)
    database = pathlib.Path(tempfile.mkdtemp(prefix="graphenedb-real-api-"))
    port = free_port()
    latency_ms: list[float] = []
    expected_links = sum(len(record["categories"]) for record in records)
    recalled_links = 0
    multi_root_records = 0
    complete_multi_root = 0
    provenance_safe = 0
    false_promotions = 0

    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as log:
        process, client = start_server(binary, database, port, log)
        imported = client.put_extraction(extraction)
        mapping = imported.data["external_to_node_id"]
        replay = client.put_extraction(extraction)

        for record in records:
            expected = {
                mapping[f"category/{category}"]
                for category in record["categories"]
            }
            if len(expected) > 1:
                multi_root_records += 1
            started = time.perf_counter()
            response = client.reason_dialectic(
                record["title"],
                signature=signature_for_record(record["uuid"]),
                mode="empirical",
                semantic_candidates=1,
                max_hops=2,
                max_paths=16,
                max_paths_per_root=4,
            )
            latency_ms.append((time.perf_counter() - started) * 1000.0)
            bundle = (
                response.data["reopened_bundle"]
                if response.data["has_reopened_bundle"]
                else response.data["initial_bundle"]
            )
            returned = {root["root_node"] for root in bundle["roots"]}
            recalled = len(expected.intersection(returned))
            recalled_links += recalled
            if len(expected) > 1 and recalled == len(expected):
                complete_multi_root += 1
            if recalled and all(
                root["provenance_risk"] == 0
                for root in bundle["roots"]
                if root["root_node"] in expected
            ):
                provenance_safe += 1
            synthesis = response.data["synthesis"]
            if synthesis["has_answer"] and synthesis["primary_node"] not in expected:
                false_promotions += 1

        provenance_validation = client.validate_provenance()
        storage_validation = client.validate()
        stop_server(process)

        process, restarted = start_server(binary, database, port, log)
        restart_replay = restarted.put_extraction(extraction)
        restart_valid = restarted.validate()
        stop_server(process)

    micro_recall = recalled_links / expected_links
    multi_root_rate = (
        complete_multi_root / multi_root_records if multi_root_records else 1.0
    )
    result = {
        "real_postmortem_api_evaluation": True,
        "claim_scope": "annotated_graph_retrieval_not_autonomous_rca",
        "records": len(records),
        "category_roots": len(
            {category for record in records for category in record["categories"]}
        ),
        "annotated_category_links": expected_links,
        "atomic_insert_status": imported.status,
        "inserted_nodes": len(imported.data["inserted_node_ids"]),
        "inserted_edges": len(imported.data["inserted_edge_ids"]),
        "same_process_replay_no_writes": (
            replay.status == 200
            and replay.data["idempotent_replay"] is True
        ),
        "restart_replay_no_writes": (
            restart_replay.status == 200
            and restart_replay.data["idempotent_replay"] is True
        ),
        "dialectic_category_micro_recall": micro_recall,
        "dialectic_complete_multi_root_rate": multi_root_rate,
        "dialectic_provenance_safe_record_rate": provenance_safe / len(records),
        "dialectic_false_promotion_rate": false_promotions / len(records),
        "api_dialectic_p95_ms": percentile(latency_ms, 95),
        "provenance_validation_ok": provenance_validation.data["ok"],
        "storage_validation_ok": storage_validation.data["ok"],
        "restart_validation_ok": restart_valid.data["ok"],
    }
    result["ok"] = (
        result["atomic_insert_status"] == 201
        and result["inserted_nodes"] == len(extraction["nodes"])
        and result["inserted_edges"] == len(extraction["relations"])
        and result["same_process_replay_no_writes"]
        and result["restart_replay_no_writes"]
        and micro_recall >= 0.95
        and multi_root_rate >= 0.95
        and result["dialectic_false_promotion_rate"] == 0
        and result["provenance_validation_ok"]
        and result["storage_validation_ok"]
        and result["restart_validation_ok"]
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
