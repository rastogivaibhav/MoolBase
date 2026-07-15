#!/usr/bin/env python3
import concurrent.futures
import http.client
import json
import os
import pathlib
import shutil
import socket
import subprocess
import struct
import sys
import tempfile
import time
import urllib.error
import urllib.request

binary = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/graphenedb_server").resolve()
root = pathlib.Path(tempfile.mkdtemp(prefix="gdb-server-hardening-"))
api_key = "hardening-test-key"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def request(port, method, path, payload=None, auth=True, timeout=10):
    data = None if payload is None else json.dumps(payload).encode()
    headers = {"Content-Type": "application/json"}
    if auth:
        headers["X-API-Key"] = api_key
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read()
            ctype = r.headers.get("Content-Type", "")
            return r.status, raw.decode(), ctype
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(), e.headers.get("Content-Type", "")


def start_server(dbdir, port, extra=None):
    cmd = [str(binary), str(dbdir), "16", str(port), "--physical-lattice-primary", "--physical-lattice-radius", "8",
           "--expected-max-nodes", "200", "--api-key", api_key, "--workers", "2", "--queue-capacity", "4",
           "--rate-limit-rps", "10000", "--rate-limit-burst", "10000", "--max-request-bytes", "2048",
           "--socket-timeout-seconds", "2"]
    if extra:
        cmd.extend(extra)
    log = open(dbdir.parent / (dbdir.name + ".log"), "w+")
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=log, text=True)
    for _ in range(100):
        if p.poll() is not None:
            log.flush(); log.seek(0)
            raise RuntimeError(f"server exited: {log.read()}")
        try:
            if request(port, "GET", "/v1/health", auth=False)[0] == 200:
                return p, log
        except Exception:
            pass
        time.sleep(0.05)
    p.kill(); raise RuntimeError("server did not become healthy")


def stop_server(p, log):
    p.terminate()
    try: p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill(); p.wait(timeout=5)
    log.flush(); log.seek(0)
    text = log.read()
    log.close()
    return text

results = {}
try:
    # Secure startup gates.
    p = subprocess.run([str(binary), str(root / "insecure"), "16", str(free_port()), "--bind-address", "0.0.0.0"], capture_output=True, text=True, timeout=10)
    results["public_bind_rejected"] = p.returncode == 2 and "refusing non-loopback bind" in p.stderr
    p = subprocess.run([str(binary), str(root / "capacity"), "16", str(free_port()), "--physical-lattice-primary", "--physical-lattice-radius", "2", "--expected-max-nodes", "100"], capture_output=True, text=True, timeout=10)
    results["capacity_preflight_rejected"] = p.returncode == 2 and "minimum radius" in p.stderr

    # Main API hardening path.
    dbdir = root / "main"; dbdir.mkdir()
    port = free_port(); proc, log = start_server(dbdir, port)
    results["ready"] = request(port, "GET", "/v1/ready", auth=False)[0] == 200
    results["auth_required"] = request(port, "GET", "/v1/metrics", auth=False)[0] == 401
    status, capacity, _ = request(port, "GET", "/v1/admin/capacity")
    results["capacity_endpoint"] = status == 200 and json.loads(capacity)["capacity"] == 217
    status, prom, ctype = request(port, "GET", "/v1/metrics/prometheus")
    results["prometheus_metrics"] = status == 200 and "graphenedb_http_requests_total" in prom and ctype.startswith("text/plain")

    # A client that resets the socket before reading must not terminate the server.
    reset_client = socket.create_connection(("127.0.0.1", port), timeout=2)
    reset_client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    reset_client.sendall((
        "GET /v1/metrics HTTP/1.1\r\nHost: localhost\r\n"
        f"X-API-Key: {api_key}\r\nConnection: close\r\n\r\n"
    ).encode())
    reset_client.close()
    time.sleep(0.1)
    results["client_disconnect_survival"] = proc.poll() is None and request(port, "GET", "/v1/health", auth=False)[0] == 200

    status, _, _ = request(port, "POST", "/v1/nodes", {"text": "x" * 4096})
    results["request_size_limit"] = status == 413
    ids = []
    for i in range(20):
        status, body, _ = request(port, "POST", "/v1/nodes", {"text": f"hardening node {i}", "source": "hardening"})
        if status == 201: ids.append(json.loads(body)["id"])
    results["writes"] = len(ids) == 20
    results["validate"] = json.loads(request(port, "POST", "/v1/admin/validate", {})[1])["ok"]
    results["checkpoint"] = json.loads(request(port, "POST", "/v1/admin/checkpoint", {})[1])["ok"]
    logs = stop_server(proc, log)
    structured = []
    invalid_json_lines = 0
    for line in logs.splitlines():
        if not line.strip():
            continue
        try:
            obj = json.loads(line)
            if obj.get("event") == "http_request": structured.append(obj)
        except Exception:
            invalid_json_lines += 1
    results["structured_json_logs"] = invalid_json_lines == 0 and bool(structured) and all("request_id" in x and "duration_us" in x for x in structured)

    # Restart persistence.
    proc, log = start_server(dbdir, port)
    status, body, _ = request(port, "GET", f"/v1/nodes/{ids[-1]}")
    results["restart_persistence"] = status == 200 and json.loads(body)["content"] == "hardening node 19"
    stop_server(proc, log)

    # Per-client token bucket.
    ratedir = root / "rate"; ratedir.mkdir(); rate_port = free_port()
    proc, log = start_server(ratedir, rate_port, ["--rate-limit-rps", "0.1", "--rate-limit-burst", "2"])
    statuses = [request(rate_port, "GET", "/v1/metrics")[0] for _ in range(5)]
    results["rate_limiting"] = statuses.count(429) >= 3
    stop_server(proc, log)

    # Bounded queue: one active slow request, one queued slow request, then overload rejection.
    qdir = root / "queue"; qdir.mkdir(); qport = free_port()
    proc, log = start_server(qdir, qport, ["--workers", "1", "--queue-capacity", "1", "--socket-timeout-seconds", "3"])
    slow = []
    for _ in range(2):
        s = socket.create_connection(("127.0.0.1", qport), timeout=2)
        s.sendall(b"POST /v1/nodes HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n")
        slow.append(s)
        time.sleep(0.15)
    s3 = socket.create_connection(("127.0.0.1", qport), timeout=2)
    s3.sendall(b"GET /v1/health HTTP/1.1\r\nHost: localhost\r\n\r\n")
    data = s3.recv(1024).decode(errors="replace")
    s3.close()
    results["bounded_queue_rejection"] = data.startswith("HTTP/1.1 503") and "server_overloaded" in data
    for s in slow: s.close()
    stop_server(proc, log)

    results["ok"] = all(results.values())
    print(json.dumps(results, indent=2, sort_keys=True))
    sys.exit(0 if results["ok"] else 1)
finally:
    shutil.rmtree(root, ignore_errors=True)
