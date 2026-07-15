#!/usr/bin/env python3
import argparse
import concurrent.futures
import json
import pathlib
import random
import shutil
import socket
import statistics
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.request

parser = argparse.ArgumentParser(description="GrapheneDB mixed HTTP server soak")
parser.add_argument("--binary", required=True)
parser.add_argument("--seconds", type=int, default=60)
parser.add_argument("--clients", type=int, default=8)
parser.add_argument("--target-rps", type=float, default=100.0, help="Approximate aggregate request rate")
parser.add_argument("--output")
parser.add_argument("--keep-db", action="store_true")
args = parser.parse_args()

api_key = "soak-test-key"
root = pathlib.Path(tempfile.mkdtemp(prefix="gdb-server-soak-"))
dbdir = root / "db"; dbdir.mkdir()
with socket.socket() as s:
    s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]
log_path = root / "server.jsonl"
log = open(log_path, "w+")
cmd = [args.binary, str(dbdir), "32", str(port), "--physical-lattice-primary", "--physical-lattice-radius", "128",
       "--expected-max-nodes", "40000", "--api-key", api_key, "--workers", str(max(4, args.clients)),
       "--queue-capacity", "2048", "--rate-limit-rps", "100000", "--rate-limit-burst", "100000",
       "--wal-rotate-bytes", str(16 * 1024 * 1024)]
proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=log, text=True)


def call(method, path, payload=None, timeout=20):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data,
                                 headers={"Content-Type":"application/json", "X-API-Key":api_key}, method=method)
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read().decode()
            return r.status, raw, (time.perf_counter()-start)*1000
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(), (time.perf_counter()-start)*1000

for _ in range(200):
    if proc.poll() is not None: raise SystemExit("server exited during startup")
    try:
        req = urllib.request.urlopen(f"http://127.0.0.1:{port}/v1/health", timeout=1)
        if req.status == 200: break
    except Exception: time.sleep(.05)
else: raise SystemExit("server did not start")

stop = threading.Event()
latencies=[]; failures=[]; ids=[]; lock=threading.Lock(); counters={"writes":0,"reads":0,"searches":0,"lattice":0,"checkpoints":0}


def worker(seed):
    rng=random.Random(seed)
    local_ids=[]
    per_client_interval = args.clients / max(1.0, args.target_rps)
    while not stop.is_set():
        iteration_started = time.monotonic()
        op=rng.random()
        try:
            if op < .55:
                payload={"text":f"soak {seed} {time.time_ns()}","source":"server-soak","service":f"svc-{seed%5}"}
                st,body,ms=call("POST","/v1/nodes",payload)
                if st==201:
                    nid=json.loads(body)["id"]; local_ids.append(nid)
                    with lock: ids.append(nid); counters["writes"]+=1; latencies.append(ms)
                else: failures.append((st,body[:100]))
            elif op < .75 and local_ids:
                nid=rng.choice(local_ids); st,body,ms=call("GET",f"/v1/nodes/{nid}")
                with lock: counters["reads"]+=1; latencies.append(ms)
                if st!=200: failures.append((st,body[:100]))
            elif op < .9:
                st,body,ms=call("POST","/v1/search/hybrid",{"query":"soak service incident","top_k":5})
                with lock: counters["searches"]+=1; latencies.append(ms)
                if st!=200: failures.append((st,body[:100]))
            elif local_ids:
                nid=rng.choice(local_ids); st,body,ms=call("GET",f"/v1/search/lattice?node_id={nid}&hops=2")
                with lock: counters["lattice"]+=1; latencies.append(ms)
                if st!=200: failures.append((st,body[:100]))
        except Exception as exc:
            failures.append(("exception",str(exc)[:100]))
        remaining = per_client_interval - (time.monotonic() - iteration_started)
        if remaining > 0:
            stop.wait(remaining)

threads=[threading.Thread(target=worker,args=(i,),daemon=True) for i in range(args.clients)]
for t in threads:t.start()
start=time.monotonic(); next_checkpoint=start+max(10,args.seconds//4)
while time.monotonic()-start < args.seconds:
    time.sleep(.5)
    if time.monotonic() >= next_checkpoint:
        st,body,ms=call("POST","/v1/admin/checkpoint",{},timeout=120)
        counters["checkpoints"]+=1
        if st!=200: failures.append((st,body[:100]))
        next_checkpoint=time.monotonic()+max(10,args.seconds//4)
stop.set()
for t in threads:t.join(timeout=10)

validate_status,validate_body,_=call("POST","/v1/admin/validate",{},timeout=120)
checkpoint_status,checkpoint_body,_=call("POST","/v1/admin/checkpoint",{},timeout=120)
metrics_status,metrics_body,_=call("GET","/v1/metrics")
capacity_status,capacity_body,_=call("GET","/v1/admin/capacity")
proc.terminate(); proc.wait(timeout=30); log.flush(); log.close()

# Restart proof.
log2=open(root/"restart.jsonl","w+")
proc2=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=log2,text=True)
for _ in range(400):
    try:
        if urllib.request.urlopen(f"http://127.0.0.1:{port}/v1/health",timeout=1).status==200: break
    except Exception: time.sleep(.05)
restart_samples=[]
for nid in (ids[:2]+ids[-2:] if ids else []):
    st,body,ms=call("GET",f"/v1/nodes/{nid}")
    restart_samples.append({"id":nid,"status":st,"latency_ms":ms})
restart_validate=call("POST","/v1/admin/validate",{},timeout=120)
proc2.terminate(); proc2.wait(timeout=30); log2.close()

lat_sorted=sorted(latencies)
def pct(p):
    if not lat_sorted:return 0
    return lat_sorted[min(len(lat_sorted)-1,int((len(lat_sorted)-1)*p/100))]
result={
    "ok": not failures and validate_status==200 and json.loads(validate_body).get("ok") and checkpoint_status==200
          and restart_validate[0]==200 and json.loads(restart_validate[1]).get("ok")
          and all(x["status"]==200 for x in restart_samples),
    "seconds":args.seconds,"clients":args.clients,"target_rps":args.target_rps,"counters":counters,"failure_count":len(failures),"first_failures":failures[:10],
    "latency_ms":{"p50":pct(50),"p95":pct(95),"p99":pct(99),"max":max(latencies) if latencies else 0,"samples":len(latencies)},
    "metrics":json.loads(metrics_body) if metrics_status==200 else metrics_body,
    "capacity":json.loads(capacity_body) if capacity_status==200 else capacity_body,
    "restart_samples":restart_samples,"restart_validate":json.loads(restart_validate[1]),
    "db_files":{p.name:p.stat().st_size for p in dbdir.iterdir() if p.is_file()},
    "note":"Use --seconds 86400 on approved hardware for the full 24-hour launch gate."
}
text=json.dumps(result,indent=2,sort_keys=True)
print(text)
if args.output:pathlib.Path(args.output).write_text(text+"\n")
if args.keep_db: print(f"kept_db={root}")
else: shutil.rmtree(root,ignore_errors=True)
raise SystemExit(0 if result["ok"] else 1)
