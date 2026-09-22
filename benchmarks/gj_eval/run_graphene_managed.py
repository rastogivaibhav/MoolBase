#!/usr/bin/env python3
"""Run one Graphene-backed GJ-Eval arm against an isolated managed server."""

from __future__ import annotations

import argparse
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
COMMAND_RUNNER = HERE / "run_command_adapter.py"
API_KEY = "gj-eval-managed-runner"


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def healthy(port: int) -> bool:
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/v1/health",
        headers={"X-API-Key": API_KEY},
    )
    try:
        with urllib.request.urlopen(req, timeout=2) as response:
            return response.status == 200
    except Exception:
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-binary", required=True)
    parser.add_argument("--worlds", required=True)
    parser.add_argument("--adapter", required=True)
    parser.add_argument("--system", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--timeout-s", type=float, default=60.0)
    args = parser.parse_args()

    work = pathlib.Path(tempfile.mkdtemp(prefix=f"gj-eval-{args.system}-"))
    process = log = None
    try:
        port = free_port()
        db = work / "db"
        db.mkdir()
        log = open(work / "server.log", "w+", encoding="utf-8")
        env = dict(os.environ)
        env["GRAPHENEDB_API_KEY"] = API_KEY
        process = subprocess.Popen(
            [
                str(pathlib.Path(args.server_binary).resolve()),
                str(db),
                "16",
                str(port),
                "--bind-address",
                "127.0.0.1",
                "--workers",
                "4",
                "--queue-capacity",
                "256",
                "--rate-limit-rps",
                "5000",
                "--rate-limit-burst",
                "5000",
            ],
            stdout=subprocess.DEVNULL,
            stderr=log,
            env=env,
            text=True,
        )
        for _ in range(400):
            if process.poll() is not None:
                log.flush()
                log.seek(0)
                raise RuntimeError(log.read())
            if healthy(port):
                break
            time.sleep(0.025)
        else:
            raise RuntimeError("GrapheneDB server did not become healthy")

        run_env = dict(os.environ)
        run_env["GRAPHENEDB_URL"] = f"http://127.0.0.1:{port}"
        run_env["GRAPHENEDB_API_KEY"] = API_KEY
        subprocess.run(
            [
                sys.executable,
                str(COMMAND_RUNNER),
                "--worlds",
                args.worlds,
                "--adapter-cmd",
                f"{sys.executable} {pathlib.Path(args.adapter).resolve()}",
                "--system",
                args.system,
                "--state-mode",
                "structured",
                "--timeout-s",
                str(args.timeout_s),
                "--output",
                args.output,
            ],
            check=True,
            env=run_env,
        )

        process.send_signal(signal.SIGTERM)
        process.wait(timeout=30)
        log.close()
        process = log = None
        return 0
    finally:
        if process is not None:
            process.kill()
            process.wait(timeout=10)
        if log is not None:
            log.close()
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
