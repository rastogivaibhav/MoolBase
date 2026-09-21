#!/usr/bin/env python3
"""Contract-test the GJ-Eval G6 adapter against a real GrapheneDB server."""

from __future__ import annotations

import json
import os
import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request


HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from generate_worldshift import build_world  # noqa: E402
from run_command_adapter import public_task  # noqa: E402


BINARY = pathlib.Path(sys.argv[1]).resolve()
ADAPTER = HERE / "adapters" / "graphene_http.py"
API_KEY = "gj-eval-graphene-contract"
WORK = pathlib.Path(tempfile.mkdtemp(prefix="gj-eval-graphene-contract-"))


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def get_json(port: int, path: str) -> dict:
    headers = {"Accept": "application/json", "X-API-Key": API_KEY}
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}{path}", headers=headers, method="GET"
    )
    with urllib.request.urlopen(req, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))


def start_server(port: int):
    db = WORK / "db"
    db.mkdir(parents=True)
    log = open(WORK / "server.log", "w+", encoding="utf-8")
    env = dict(os.environ)
    env["GRAPHENEDB_API_KEY"] = API_KEY
    process = subprocess.Popen(
        [
            str(BINARY), str(db), "16", str(port),
            "--bind-address", "127.0.0.1",
            "--workers", "2", "--queue-capacity", "64",
            "--rate-limit-rps", "1000", "--rate-limit-burst", "1000",
        ],
        stdout=subprocess.DEVNULL,
        stderr=log,
        env=env,
        text=True,
    )
    for _ in range(240):
        if process.poll() is not None:
            log.flush()
            log.seek(0)
            raise RuntimeError(log.read())
        try:
            status = get_json(port, "/v1/health")
            if status.get("status") == "ok":
                return process, log
        except Exception:
            pass
        time.sleep(0.025)
    process.kill()
    raise RuntimeError("GrapheneDB server did not become healthy")


def run_adapter(port: int, task: dict) -> dict:
    env = dict(os.environ)
    env["GRAPHENEDB_URL"] = f"http://127.0.0.1:{port}"
    env["GRAPHENEDB_API_KEY"] = API_KEY
    completed = subprocess.run(
        [sys.executable, str(ADAPTER)],
        input=json.dumps(task),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        check=True,
        timeout=30,
    )
    return json.loads(completed.stdout)


def main() -> int:
    process = log = None
    try:
        port = free_port()
        process, log = start_server(port)
        replay_seen = False
        contract_tasks = 0

        for world_index in range(13):
            world = build_world(1729, world_index, "development")
            valid_choices = {choice["id"] for choice in world["choices"]}
            previous_snapshot = -1

            for step in world["timeline"]:
                if contract_tasks >= 100:
                    break
                timestep = int(step["timestep"])
                task = public_task(world, timestep, "structured")
                assert "oracle" not in task
                result = run_adapter(port, task)
                contract_tasks += 1

                assert result["root_choice"] in valid_choices
                assert result["act"] in {"act", "review", "abstain"}
                assert 0.0 <= float(result["selected_confidence"]) <= 1.0

                receipt = result["receipt"]
                runtime = receipt["runtime"]
                runtime_receipt = runtime["receipt"]
                assert runtime_receipt["graphene_executed"] is True
                assert runtime_receipt["fiber_bundle_built"] is True
                assert runtime_receipt["stability_critic_executed"] is True
                assert runtime_receipt["lyapunov_trajectory_executed"] is True
                assert runtime_receipt["opposition_executed"] is True
                assert runtime_receipt["governed_projection_executed"] is True
                assert runtime_receipt["no_silent_promotion"] is True
                snapshot = int(runtime_receipt["snapshot_version"])
                assert snapshot >= previous_snapshot
                previous_snapshot = snapshot

                extraction = receipt["extraction"]
                if extraction["existing_nodes"] > 0:
                    replay_seen = True
            if contract_tasks >= 100:
                break

        assert contract_tasks == 100
        assert replay_seen, "cumulative idempotent replay was not exercised"
        metrics = get_json(port, "/v1/metrics")
        assert metrics["node_count"] > 0
        assert metrics["edge_count"] > 0

        process.send_signal(signal.SIGTERM)
        process.wait(timeout=30)
        log.close()
        process = log = None

        print(json.dumps({
            "graphene_g6_contract_test": True,
            "canonical_tasks": contract_tasks,
            "persistent_state": True,
            "idempotent_replay": True,
            "runtime_receipt": True,
        }, sort_keys=True))
        return 0
    finally:
        if process is not None:
            process.kill()
            process.wait(timeout=10)
        if log is not None:
            log.close()
        shutil.rmtree(WORK, ignore_errors=True)

