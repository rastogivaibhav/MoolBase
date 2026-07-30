import http.client, json, time, os, sys, subprocess, tempfile, threading, statistics, random, shutil, signal
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT='/mnt/data/gdb_do'
BIN=os.path.join(ROOT,'build','graphenedb_server')
PORT=int(os.environ.get('GDB_PORT','19191'))
API='bench-key'
DBDIR='/tmp/gdb_100k_server'
EVDIR='/mnt/data/gdb_next_proof_evidence'
os.makedirs(EVDIR, exist_ok=True)

def req(method,path,body=None,timeout=20):
    conn=http.client.HTTPConnection('127.0.0.1',PORT,timeout=timeout)
    headers={'x-api-key':API}
    data=None
    if body is not None:
        data=json.dumps(body,separators=(',',':')).encode()
        headers['Content-Type']='application/json'
    t=time.perf_counter()
    conn.request(method,path,data,headers)
    r=conn.getresponse(); raw=r.read(); dt=(time.perf_counter()-t)*1000
    conn.close()
    try: js=json.loads(raw.decode() or '{}')
    except Exception: js={'raw':raw.decode(errors='replace')}
    return r.status,js,dt

def start(dbdir=DBDIR):
    if os.path.exists(dbdir): shutil.rmtree(dbdir)
    os.makedirs(dbdir, exist_ok=True)
    p=subprocess.Popen([BIN,dbdir,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    deadline=time.time()+10
    while time.time()<deadline:
        try:
            s,js,dt=req('GET','/v1/health',timeout=1)
            if s==200: return p
        except Exception: time.sleep(0.05)
    raise RuntimeError('server did not start')

def stop(p):
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill(); p.wait()

def insert_one(i):
    txt=f"Graphene server 100k proof node {i}: customer incident refund escalation SKU-{i%97} region-{i%11} causal memory cell."
    s,js,dt=req('POST','/v1/nodes',{'text':txt,'source':'100k_benchmark'},timeout=30)
    if s not in (200,201): return False,dt,js
    return True,dt,js

def run_benchmark(n=100000,workers=48):
    p=start()
    lat=[]; failures=[]; t0=time.perf_counter(); done=0
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs=[ex.submit(insert_one,i) for i in range(n)]
        for fut in as_completed(futs):
            ok,dt,js=fut.result(); done+=1
            if ok: lat.append(dt)
            else: failures.append(js)
            if done%10000==0: print('inserted',done,'fail',len(failures),flush=True)
    total=time.perf_counter()-t0
    # read/lattice/hybrid samples
    read_lat=[]; lattice_lat=[]; hybrid_lat=[]
    for i in [0,n//4,n//2,3*n//4,n-1]:
        s,js,dt=req('GET',f'/v1/nodes/{i}',timeout=20); read_lat.append(dt)
        s,js,dt=req('GET',f'/v1/search/lattice?node_id={i}&hops=2',timeout=20); lattice_lat.append(dt)
    for q in ['refund escalation region','causal memory sku','customer incident before meeting']*10:
        s,js,dt=req('POST','/v1/search/hybrid',{'query':q,'top_k':10},timeout=20); hybrid_lat.append(dt)
    s,metrics,dt=req('GET','/v1/metrics',timeout=20)
    s,val,dt=req('POST','/v1/admin/validate',{},timeout=60)
    files={f:os.path.getsize(os.path.join(DBDIR,f)) for f in os.listdir(DBDIR)}
    stop(p)
    # restart check
    p=subprocess.Popen([BIN,DBDIR,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    time.sleep(1)
    restart_checks=[]
    for i in [42,n//2,n-1]:
        s,js,dt=req('GET',f'/v1/nodes/{i}',timeout=20); restart_checks.append((i,s,js.get('content','')[:60],dt))
    s,met2,dt=req('GET','/v1/metrics',timeout=20)
    stop(p)
    def pct(a,pct):
        if not a: return None
        b=sorted(a); return b[min(len(b)-1,int(len(b)*pct/100))]
    result={
      'target_nodes':n,'workers':workers,'successful_inserts':len(lat),'failures':len(failures),'total_seconds':total,'write_throughput_per_sec':len(lat)/total if total else 0,
      'insert_ms':{'p50':pct(lat,50),'p95':pct(lat,95),'p99':pct(lat,99),'max':max(lat) if lat else None},
      'read_ms':{'p50':pct(read_lat,50),'samples':read_lat},
      'lattice_ms':{'p50':pct(lattice_lat,50),'samples':lattice_lat},
      'hybrid_ms':{'p50':pct(hybrid_lat,50),'p95':pct(hybrid_lat,95),'samples':hybrid_lat[:5]},
      'metrics_before_restart':metrics,
      'metrics_after_restart':met2,
      'validate':val,
      'files':files,
      'restart_checks':restart_checks,
      'first_failures':failures[:5]
    }
    open(os.path.join(EVDIR,'server_100k_benchmark.json'),'w').write(json.dumps(result,indent=2))
    return result

if __name__=='__main__':
    n=int(sys.argv[1]) if len(sys.argv)>1 else 100000
    workers=int(sys.argv[2]) if len(sys.argv)>2 else 48
    r=run_benchmark(n,workers)
    print(json.dumps(r,indent=2)[:4000])
