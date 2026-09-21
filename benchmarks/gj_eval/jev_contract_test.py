#!/usr/bin/env python3
"""Provider-free contract test for the official Jev HTTP adapter."""

from __future__ import annotations

import json
import os
import pathlib
import socket
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any


HERE = pathlib.Path(__file__).resolve().parent
ADAPTER = HERE / "adapters" / "jev_http.py"


class Handler(BaseHTTPRequestHandler):
    requests: list[dict[str, Any]] = []

    def log_message(self, *_args: Any) -> None:
        return

    def do_POST(self) -> None:
        length = int(self.headers.get("content-length", "0"))
        body = json.loads(self.rfile.read(length).decode("utf-8"))
        assert self.headers.get("Authorization") == "Bearer contract-test-key"
        assert body["model"] == "jev-latest"
        assert set(body["questions"]) >= {"root_cause", "evidence_sufficient"}
        assert body["questions"]["root_cause"]["type"] == "choice"
        assert body["questions"]["evidence_sufficient"]["type"] == "noul"
        self.__class__.requests.append(body)

        response = {
            "model": "jev-contract-test",
            "answers": {
                "root_cause": {
                    "type": "choice",
                    "choice": "database",
                    "confidence": 0.81,
                    "probabilities": {
                        "deployment": 0.05,
                        "database": 0.84,
                        "network": 0.04,
                        "certificate": 0.02,
                        "unknown": 0.05,
                    },
                },
                "evidence_sufficient": {
                    "type": "noul",
                    "noul": 0.88,
                },
                "next_test": {
                    "type": "choice",
                    "choice": "inspect_db",
                    "confidence": 0.70,
                    "probabilities": {
                        "rollback": 0.05,
                        "inspect_db": 0.70,
                        "inspect_network": 0.05,
                        "inspect_cert": 0.05,
                        "none": 0.15,
                    },
                },
            },
            "usage": {"input_tokens": 1000, "output_tokens": 50},
        }
        encoded = json.dumps(response).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def run_adapter(task: dict[str, Any], port: int) -> dict[str, Any]:
    env = dict(os.environ)
    env["TYPESAFE_API_KEY"] = "contract-test-key"
    env["TYPESAFE_API_URL"] = f"http://127.0.0.1:{port}/v1/systemone"
    completed = subprocess.run(
        [sys.executable, str(ADAPTER)],
        input=json.dumps(task),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        check=True,
        timeout=10,
    )
    return json.loads(completed.stdout)


def task(visible_state: Any) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "world_id": "contract-world",
        "timestep": 3,
        "domain": "aiops",
        "variant": "base",
        "visible_state": visible_state,
        "choices": [
            {"id": "deployment", "label": "Deployment"},
            {"id": "database", "label": "Database"},
            {"id": "network", "label": "Network"},
            {"id": "certificate", "label": "Certificate"},
            {"id": "unknown", "label": "Unknown / insufficient evidence"},
        ],
        "tests": [
            {"id": "rollback", "label": "Roll back deployment canary", "cost": 3.0},
            {"id": "inspect_db", "label": "Inspect database saturation history", "cost": 1.0},
            {"id": "inspect_network", "label": "Inspect packet-loss history", "cost": 1.0},
            {"id": "inspect_cert", "label": "Inspect certificate validity logs", "cost": 1.0},
        ],
    }


def main() -> int:
    port = free_port()
    server = HTTPServer(("127.0.0.1", port), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        structured = run_adapter(
            task([
                {
                    "evidence_id": "e1",
                    "claim": "Database saturation preceded checkout failures.",
                    "source_id": "db-monitor",
                    "source_family": "db-monitor",
                    "supports": "database",
                    "confidence": 0.9,
                }
            ]),
            port,
        )
        raw = run_adapter(
            task(["Database saturation preceded checkout failures."]),
            port,
        )
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

    assert structured["root_choice"] == "database"
    assert structured["act"] == "act"
    assert structured["selected_confidence"] == 0.84
    assert structured["choice_probabilities"]["database"] == 0.84
    assert structured["receipt"]["jev_native_confidence"] == 0.81
    assert structured["receipt"]["evidence_sufficient_noul"] == 0.88
    assert abs(structured["provider_cost"] - 0.000000042) < 1e-12
    assert raw["root_choice"] == "database"

    assert len(Handler.requests) == 2
    assert isinstance(Handler.requests[0]["state"], dict)
    assert isinstance(Handler.requests[1]["state"], str)
    assert "observations" in Handler.requests[0]["state"]
    assert "source_family" not in Handler.requests[1]["state"]

    print(json.dumps({
        "jev_contract_test": True,
        "official_shape": True,
        "structured_state": True,
        "raw_state": True,
        "probability_calibration_field": True,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
