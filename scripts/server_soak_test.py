#!/usr/bin/env python3
import argparse
from collections import deque
import json
import pathlib
import random
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.request

MAX_FAILURE_SAMPLES = 10
MAX_LATENCY_SAMPLES = 200_000
LOCAL_ID_WINDOW = 4096


def parse_args():
    parser = argparse.ArgumentParser(description="GrapheneDB mixed HTTP server soak")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--binary", help="Path to graphenedb_server for self-hosted soak mode")
    mode.add_argument("--base-url", help="Base URL for an already running server, for example http://127.0.0.1:18080")
    parser.add_argument("--api-key", default="soak-test-key")
    parser.add_argument("--container-name", help="Docker container name to restart for external-mode durability proof")
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--clients", type=int, default=8)
    parser.add_argument("--target-rps", type=float, default=100.0, help="Approximate aggregate request rate")
    parser.add_argument("--output")
    parser.add_argument("--keep-db", action="store_true")
    return parser.parse_args()


def call(base_url, api_key, method, path, payload=None, timeout=20):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(
        f"{base_url}{path}",
        data=data,
        headers={"Content-Type": "application/json", "X-API-Key": api_key},
        method=method,
    )
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            raw = response.read().decode()
            return response.status, raw, (time.perf_counter() - start) * 1000
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode(), (time.perf_counter() - start) * 1000


def wait_healthy(base_url, attempts=200, interval=0.25):
    for _ in range(attempts):
        try:
            with urllib.request.urlopen(f"{base_url}/v1/health", timeout=1) as response:
                if response.status == 200:
                    return True
        except Exception:
            time.sleep(interval)
    return False


def start_local_server(binary, api_key, clients):
    root = pathlib.Path(tempfile.mkdtemp(prefix="gdb-server-soak-"))
    dbdir = root / "db"
    dbdir.mkdir()
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    log_path = root / "server.jsonl"
    log = open(log_path, "w+")
    cmd = [
        binary,
        str(dbdir),
        "32",
        str(port),
        "--physical-lattice-primary",
        "--physical-lattice-radius",
        "128",
        "--expected-max-nodes",
        "40000",
        "--api-key",
        api_key,
        "--workers",
        str(max(4, clients)),
        "--queue-capacity",
        "2048",
        "--rate-limit-rps",
        "100000",
        "--rate-limit-burst",
        "100000",
        "--wal-rotate-bytes",
        str(16 * 1024 * 1024),
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=log, text=True)
    base_url = f"http://127.0.0.1:{port}"
    if proc.poll() is not None or not wait_healthy(base_url):
        raise SystemExit("server did not start")
    return {
        "mode": "binary",
        "root": root,
        "dbdir": dbdir,
        "log": log,
        "cmd": cmd,
        "proc": proc,
        "base_url": base_url,
    }


def restart_local_server(state):
    state["log2"] = open(state["root"] / "restart.jsonl", "w+")
    state["proc2"] = subprocess.Popen(state["cmd"], stdout=subprocess.DEVNULL, stderr=state["log2"], text=True)
    if not wait_healthy(state["base_url"], attempts=400):
        raise SystemExit("restarted server did not become healthy")


def stop_local_server(state):
    proc = state.get("proc")
    if proc is not None:
        proc.terminate()
        proc.wait(timeout=30)
    if "log" in state:
        state["log"].flush()
        state["log"].close()


def stop_restarted_local_server(state):
    proc = state.get("proc2")
    if proc is not None:
        proc.terminate()
        proc.wait(timeout=30)
    log = state.get("log2")
    if log is not None:
        log.flush()
        log.close()


def container_runtime_state(container_name):
    completed = subprocess.run(
        ["docker", "inspect", container_name],
        check=True,
        capture_output=True,
        text=True,
    )
    inspected = json.loads(completed.stdout)[0]
    state = inspected["State"]
    return {
        "status": state.get("Status"),
        "running": bool(state.get("Running")),
        "oom_killed": bool(state.get("OOMKilled")),
        "exit_code": int(state.get("ExitCode", 0)),
        "restart_count": int(inspected.get("RestartCount", 0)),
        "started_at": state.get("StartedAt"),
        "finished_at": state.get("FinishedAt"),
    }


def restart_container(container_name):
    started = time.monotonic()
    subprocess.run(
        ["docker", "stop", "--timeout", "300", container_name],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    stopped = container_runtime_state(container_name)
    if stopped["oom_killed"] or stopped["exit_code"] != 0:
        raise RuntimeError(
            "container did not stop cleanly: "
            f"exit={stopped['exit_code']} oom={stopped['oom_killed']}"
        )
    subprocess.run(
        ["docker", "start", container_name],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    return {
        "graceful_stop_seconds": time.monotonic() - started,
        "stopped_state": stopped,
    }


def main():
    args = parse_args()
    state = None
    base_url = args.base_url.rstrip("/") if args.base_url else None

    if args.binary:
        state = start_local_server(args.binary, args.api_key, args.clients)
        base_url = state["base_url"]
    else:
        if not wait_healthy(base_url):
            raise SystemExit(f"server not healthy at {base_url}")

    stop = threading.Event()
    latencies = []
    failures = []
    failure_count = 0
    latency_population = 0
    first_ids = []
    last_ids = deque(maxlen=2)
    lock = threading.Lock()
    counters = {"writes": 0, "reads": 0, "searches": 0, "lattice": 0, "checkpoints": 0}
    container_baseline = (
        container_runtime_state(args.container_name) if args.container_name else None
    )
    container_final = container_baseline

    def record_failure(failure):
        nonlocal failure_count
        with lock:
            failure_count += 1
            if len(failures) < MAX_FAILURE_SAMPLES:
                failures.append(failure)
        stop.set()

    def record_operation(counter, latency_ms, rng, node_id=None):
        nonlocal latency_population
        with lock:
            counters[counter] += 1
            latency_population += 1
            if len(latencies) < MAX_LATENCY_SAMPLES:
                latencies.append(latency_ms)
            else:
                replacement = rng.randrange(latency_population)
                if replacement < MAX_LATENCY_SAMPLES:
                    latencies[replacement] = latency_ms
            if node_id is not None:
                if len(first_ids) < 2:
                    first_ids.append(node_id)
                last_ids.append(node_id)

    def worker(seed):
        rng = random.Random(seed)
        local_ids = deque(maxlen=LOCAL_ID_WINDOW)
        per_client_interval = args.clients / max(1.0, args.target_rps)
        while not stop.is_set():
            iteration_started = time.monotonic()
            op = rng.random()
            try:
                if op < 0.55:
                    payload = {
                        "text": f"soak {seed} {time.time_ns()}",
                        "source": "server-soak",
                        "service": f"svc-{seed % 5}",
                    }
                    status, body, latency_ms = call(base_url, args.api_key, "POST", "/v1/nodes", payload)
                    if status == 201:
                        node_id = json.loads(body)["id"]
                        local_ids.append(node_id)
                        record_operation("writes", latency_ms, rng, node_id)
                    else:
                        record_failure((status, body[:100]))
                elif op < 0.75 and local_ids:
                    node_id = rng.choice(local_ids)
                    status, body, latency_ms = call(base_url, args.api_key, "GET", f"/v1/nodes/{node_id}")
                    if status != 200:
                        record_failure((status, body[:100]))
                    else:
                        record_operation("reads", latency_ms, rng)
                elif op < 0.9:
                    status, body, latency_ms = call(
                        base_url,
                        args.api_key,
                        "POST",
                        "/v1/search/hybrid",
                        {"query": "soak service incident", "top_k": 5},
                    )
                    if status != 200:
                        record_failure((status, body[:100]))
                    else:
                        record_operation("searches", latency_ms, rng)
                elif local_ids:
                    node_id = rng.choice(local_ids)
                    status, body, latency_ms = call(
                        base_url,
                        args.api_key,
                        "GET",
                        f"/v1/search/lattice?node_id={node_id}&hops=2",
                    )
                    if status != 200:
                        record_failure((status, body[:100]))
                    else:
                        record_operation("lattice", latency_ms, rng)
            except Exception as exc:
                record_failure(("exception", str(exc)[:100]))
            remaining = per_client_interval - (time.monotonic() - iteration_started)
            if remaining > 0:
                stop.wait(remaining)

    threads = [threading.Thread(target=worker, args=(i,), daemon=True) for i in range(args.clients)]
    for thread in threads:
        thread.start()

    start = time.monotonic()
    next_checkpoint = start + max(10, args.seconds // 4)
    next_container_check = start + 1
    while time.monotonic() - start < args.seconds and not stop.is_set():
        time.sleep(0.5)
        if args.container_name and time.monotonic() >= next_container_check:
            try:
                container_final = container_runtime_state(args.container_name)
                if (
                    not container_final["running"]
                    or container_final["oom_killed"]
                    or container_final["restart_count"]
                    != container_baseline["restart_count"]
                ):
                    record_failure(
                        (
                            "container_runtime_changed",
                            {
                                "baseline": container_baseline,
                                "observed": container_final,
                            },
                        )
                    )
                    break
            except Exception as exc:
                record_failure(("container_inspect_failed", str(exc)[:200]))
                break
            next_container_check = time.monotonic() + 5
        if time.monotonic() >= next_checkpoint:
            try:
                status, body, _ = call(
                    base_url,
                    args.api_key,
                    "POST",
                    "/v1/admin/checkpoint",
                    {},
                    timeout=120,
                )
                counters["checkpoints"] += 1
                if status != 200:
                    record_failure((status, body[:100]))
            except Exception as exc:
                record_failure(("checkpoint_exception", str(exc)[:200]))
            next_checkpoint = time.monotonic() + max(10, args.seconds // 4)

    stop.set()
    for thread in threads:
        thread.join(timeout=10)

    def final_call(method, path, payload=None, timeout=120):
        try:
            return call(
                base_url,
                args.api_key,
                method,
                path,
                payload,
                timeout=timeout,
            )
        except Exception as exc:
            return 0, json.dumps({"error": str(exc)}), 0

    validate_status, validate_body, _ = final_call(
        "POST", "/v1/admin/validate", {}, timeout=120
    )
    checkpoint_status, checkpoint_body, _ = final_call(
        "POST", "/v1/admin/checkpoint", {}, timeout=120
    )
    metrics_status, metrics_body, _ = final_call("GET", "/v1/metrics")
    capacity_status, capacity_body, _ = final_call("GET", "/v1/admin/capacity")

    if state is not None:
        stop_local_server(state)

    restart_samples = []
    restart_validate = {"ok": True, "skipped": True}
    operational_restart = {"skipped": True}
    restart_ok = True

    try:
        if failure_count:
            restart_ok = False
            restart_validate = {"ok": False, "skipped": True, "reason": "workload_failed"}
        elif state is not None:
            restart_local_server(state)
        elif args.container_name:
            operational_restart = restart_container(args.container_name)
            if not wait_healthy(base_url, attempts=1200):
                raise RuntimeError("restarted container did not become healthy")

        if not failure_count and (state is not None or args.container_name):
            sample_ids = list(dict.fromkeys(first_ids + list(last_ids)))
            for node_id in sample_ids:
                status, _, latency_ms = call(base_url, args.api_key, "GET", f"/v1/nodes/{node_id}")
                restart_samples.append({"id": node_id, "status": status, "latency_ms": latency_ms})
            restart_status, restart_body, _ = call(base_url, args.api_key, "POST", "/v1/admin/validate", {}, timeout=120)
            restart_validate = json.loads(restart_body) if restart_status == 200 else {"ok": False, "raw": restart_body}
            restart_ok = restart_status == 200 and restart_validate.get("ok") and all(
                sample["status"] == 200 for sample in restart_samples
            )
            if args.container_name:
                container_final = container_runtime_state(args.container_name)
                operational_restart["post_start_state"] = container_final
    except Exception as exc:
        restart_ok = False
        restart_validate = {"ok": False, "error": str(exc)}
    finally:
        if state is not None:
            stop_restarted_local_server(state)

    lat_sorted = sorted(latencies)

    def pct(value):
        if not lat_sorted:
            return 0
        return lat_sorted[min(len(lat_sorted) - 1, int((len(lat_sorted) - 1) * value / 100))]

    validate_ok = validate_status == 200 and json.loads(validate_body).get("ok")
    checkpoint_ok = checkpoint_status == 200

    db_files = {}
    if state is not None:
        db_files = {path.name: path.stat().st_size for path in state["dbdir"].iterdir() if path.is_file()}

    result = {
        "ok": failure_count == 0 and validate_ok and checkpoint_ok and restart_ok,
        "mode": "binary" if state is not None else "external",
        "base_url": base_url,
        "container_name": args.container_name,
        "seconds": args.seconds,
        "elapsed_seconds": time.monotonic() - start,
        "clients": args.clients,
        "target_rps": args.target_rps,
        "counters": counters,
        "failure_count": failure_count,
        "first_failures": failures,
        "latency_ms": {
            "p50": pct(50),
            "p95": pct(95),
            "p99": pct(99),
            "max": max(latencies) if latencies else 0,
            "samples": len(latencies),
            "population_samples": latency_population,
            "sampling": (
                "all"
                if latency_population <= MAX_LATENCY_SAMPLES
                else "bounded_reservoir"
            ),
        },
        "metrics": json.loads(metrics_body) if metrics_status == 200 else metrics_body,
        "capacity": json.loads(capacity_body) if capacity_status == 200 else capacity_body,
        "restart_samples": restart_samples,
        "restart_validate": restart_validate,
        "operational_restart": operational_restart,
        "container_baseline": container_baseline,
        "container_final": container_final,
        "db_files": db_files,
        "note": "Use --seconds 86400 on approved hardware for the full 24-hour launch gate.",
    }

    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.output:
        pathlib.Path(args.output).write_text(text + "\n")

    if state is not None:
        if args.keep_db:
            print(f"kept_db={state['root']}")
        else:
            shutil.rmtree(state["root"], ignore_errors=True)

    raise SystemExit(0 if result["ok"] else 1)


if __name__ == "__main__":
    main()
