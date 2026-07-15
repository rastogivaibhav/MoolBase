import http.client,json,time,os,subprocess,signal
PORT=19555; API='bench-key'; BIN='/mnt/data/gdb_do/build/graphenedb_server'; D='/tmp/gdb_bulk_100k'; E='/mnt/data/gdb_next_proof_evidence'; os.makedirs(E,exist_ok=True)
p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
def req(m,path,body=None,timeout=120):
 c=http.client.HTTPConnection('127.0.0.1',PORT,timeout=timeout); h={'x-api-key':API}; data=None
 if body is not None: data=json.dumps(body,separators=(',',':')).encode(); h['Content-Type']='application/json'
 t=time.perf_counter(); c.request(m,path,data,h); r=c.getresponse(); raw=r.read(); c.close(); dt=(time.perf_counter()-t)*1000
 try: js=json.loads(raw.decode() or '{}')
 except: js={'raw':raw.decode(errors='replace')}
 return r.status,js,dt
for i in range(200):
 try:
  s,js,dt=req('GET','/v1/health',timeout=1)
  if s==200: print('started',i,dt); break
 except Exception as e: time.sleep(.1)
else:
 print('stderr',p.stderr.read()[-2000:]); raise RuntimeError('no start')
def pct(a,x):
 b=sorted(a); return b[min(len(b)-1,int(len(b)*x/100))]
reads=[]; latt=[]; hybr=[]; samples=[]
for i in [0,1,42,9999,50000,99999]:
 a=req('GET',f'/v1/nodes/{i}',timeout=60); reads.append(a[2]); samples.append((i,a[0],a[1].get('content','')[:70],a[2]))
 b=req('GET',f'/v1/search/lattice?node_id={i}&hops=2',timeout=60); latt.append(b[2])
for j in range(10):
 hybr.append(req('POST','/v1/search/hybrid',{'query':'refund escalation causal memory customer incident','top_k':10},timeout=120)[2])
metrics=req('GET','/v1/metrics',timeout=30)[1]
inspect=req('GET','/v1/admin/inspect',timeout=30)[1]
# skip full validate if too slow? Run it now.
val_status,val_js,val_dt=req('POST','/v1/admin/validate',{},timeout=300)
files={f:os.path.getsize(os.path.join(D,f)) for f in os.listdir(D)}
p.terminate(); p.wait(timeout=10)
res={'read_ms':{'p50':pct(reads,50),'samples':reads},'lattice_ms':{'p50':pct(latt,50),'samples':latt},'hybrid_ms':{'p50':pct(hybr,50),'p95':pct(hybr,95),'samples':hybr},'samples':samples,'metrics':metrics,'inspect':inspect,'validate':{'status':val_status,'dt_ms':val_dt,'body':val_js},'files':files}
open(E+'/server_bulk_100k_postchecks.json','w').write(json.dumps(res,indent=2))
print(json.dumps(res,indent=2)[:5000])
