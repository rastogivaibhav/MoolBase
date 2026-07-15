import http.client,json,time,os,subprocess,shutil,signal
PORT=19444; API='bench-key'; BIN='/mnt/data/gdb_do/build/graphenedb_server'; D='/tmp/gdb_bulk_100k'; E='/mnt/data/gdb_next_proof_evidence'; os.makedirs(E,exist_ok=True)
shutil.rmtree(D,ignore_errors=True); os.makedirs(D,exist_ok=True)
p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
def req(m,path,body=None,timeout=60):
 c=http.client.HTTPConnection('127.0.0.1',PORT,timeout=timeout); h={'x-api-key':API}; data=None
 if body is not None: data=json.dumps(body,separators=(',',':')).encode(); h['Content-Type']='application/json'
 t=time.perf_counter(); c.request(m,path,data,h); r=c.getresponse(); raw=r.read(); c.close(); dt=(time.perf_counter()-t)*1000
 try: js=json.loads(raw.decode() or '{}')
 except: js={'raw':raw.decode(errors='replace')}
 return r.status,js,dt
for _ in range(100):
 try:
  if req('GET','/v1/health',timeout=1)[0]==200: break
 except Exception: time.sleep(.05)
else: raise RuntimeError('server failed')
# Bulk ingest in chunks so the API remains responsive and evidence has checkpoints.
chunks=[]; total=100000; chunk=10000; t0=time.perf_counter()
for base in range(0,total,chunk):
 s,js,dt=req('POST','/v1/nodes/bulk',{'count':chunk,'prefix':f'GrapheneDB 100k benchmark bulk chunk {base} customer incident refund escalation causal memory','source':'bulk_100k'},timeout=180)
 chunks.append({'base':base,'status':s,'dt_ms':dt,'response':js})
 print('chunk',base,s,dt,js,flush=True)
 if s not in (200,201): break
elapsed=time.perf_counter()-t0
# query samples
def pct(a,x):
 if not a: return None
 b=sorted(a); return b[min(len(b)-1,int(len(b)*x/100))]
reads=[]; latt=[]; hybr=[]
for i in [0,1,42,9999,50000,99999]:
 s,js,dt=req('GET',f'/v1/nodes/{i}',timeout=30); reads.append(dt)
 s,js,dt=req('GET',f'/v1/search/lattice?node_id={i}&hops=2',timeout=30); latt.append(dt)
for i in range(30):
 s,js,dt=req('POST','/v1/search/hybrid',{'query':'refund escalation causal memory customer incident','top_k':10},timeout=60); hybr.append(dt)
metrics=req('GET','/v1/metrics',timeout=30)[1]
validate=req('POST','/v1/admin/validate',{},timeout=120)[1]
inspect=req('GET','/v1/admin/inspect',timeout=30)[1]
files={f:os.path.getsize(os.path.join(D,f)) for f in os.listdir(D)}
p.terminate(); p.wait(timeout=10)
# restart persistence
p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
time.sleep(2)
restart=[]
for i in [42,50000,99999]: restart.append({'id':i,'result':req('GET',f'/v1/nodes/{i}',timeout=30)})
metrics_restart=req('GET','/v1/metrics',timeout=30)[1]
validate_restart=req('POST','/v1/admin/validate',{},timeout=120)[1]
p.terminate(); p.wait(timeout=10)
res={'target_nodes':total,'chunk_size':chunk,'chunks':chunks,'total_seconds':elapsed,'bulk_throughput_per_sec':total/elapsed,
     'read_ms':{'p50':pct(reads,50),'samples':reads},'lattice_ms':{'p50':pct(latt,50),'samples':latt},'hybrid_ms':{'p50':pct(hybr,50),'p95':pct(hybr,95),'samples':hybr},
     'metrics_before_restart':metrics,'metrics_after_restart':metrics_restart,'validate_before_restart':validate,'validate_after_restart':validate_restart,'inspect':inspect,'files':files,'restart_checks':restart}
open(E+'/server_bulk_100k_benchmark.json','w').write(json.dumps(res,indent=2))
print(json.dumps(res,indent=2)[:5000])
