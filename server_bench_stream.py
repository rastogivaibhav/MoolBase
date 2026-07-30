import http.client,json,time,os,subprocess,shutil,threading,queue,statistics,signal,sys
ROOT='/mnt/data/gdb_do'; BIN=ROOT+'/build/graphenedb_server'; PORT=19333; API='bench-key'; D='/tmp/gdb_stream'; E='/mnt/data/gdb_next_proof_evidence'; os.makedirs(E,exist_ok=True)

def req(m,p,b=None,timeout=15):
 c=http.client.HTTPConnection('127.0.0.1',PORT,timeout=timeout); h={'x-api-key':API}; data=None
 if b is not None: data=json.dumps(b,separators=(',',':')).encode(); h['Content-Type']='application/json'
 t=time.perf_counter(); c.request(m,p,data,h); r=c.getresponse(); raw=r.read(); c.close(); dt=(time.perf_counter()-t)*1000
 try: js=json.loads(raw.decode() or '{}')
 except: js={'raw':raw.decode(errors='replace')}
 return r.status,js,dt

def wait():
 for _ in range(100):
  try:
   if req('GET','/v1/health',timeout=1)[0]==200: return
  except: time.sleep(.05)
 raise RuntimeError('no start')

def start(fresh=True):
 if fresh: shutil.rmtree(D,ignore_errors=True); os.makedirs(D,exist_ok=True)
 p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
 wait(); return p

def stop(p):
 if p.poll() is None:
  p.terminate()
  try:p.wait(timeout=5)
  except: p.kill(); p.wait()

def bench(n,workers):
 p=start(True); q=queue.Queue(maxsize=workers*4); l=[]; fails=[]; lock=threading.Lock(); progress=0
 def worker():
  nonlocal progress
  while True:
   i=q.get()
   if i is None: q.task_done(); break
   txt=f'Graphene 100k server proof node {i} refund escalation incident causal memory region-{i%13} sku-{i%97}'
   try: s,js,dt=req('POST','/v1/nodes',{'text':txt,'source':'100k_stream'},timeout=30)
   except Exception as e: s=0; js={'error':str(e)}; dt=0
   with lock:
    if s==201: l.append(dt)
    else: fails.append(js)
    progress+=1
    if progress%5000==0: print('progress',progress,'ok',len(l),'fail',len(fails),flush=True)
   q.task_done()
 threads=[threading.Thread(target=worker) for _ in range(workers)]
 for t in threads:t.start()
 t0=time.perf_counter()
 for i in range(n): q.put(i)
 for _ in threads:q.put(None)
 q.join()
 total=time.perf_counter()-t0
 for t in threads:t.join()
 def pct(a,x):
  if not a:return None
  b=sorted(a); return b[min(len(b)-1,int(len(b)*x/100))]
 samples={}
 for kind in ['read','lattice','hybrid']:
  arr=[]
  if kind=='read':
   for i in [0,n//5,n//2,n-1]: arr.append(req('GET',f'/v1/nodes/{i}',timeout=20)[2])
  elif kind=='lattice':
   for i in [0,n//5,n//2,n-1]: arr.append(req('GET',f'/v1/search/lattice?node_id={i}&hops=2',timeout=20)[2])
  else:
   for j in range(20): arr.append(req('POST','/v1/search/hybrid',{'query':'refund escalation incident causal memory','top_k':10},timeout=20)[2])
  samples[kind]=arr
 met=req('GET','/v1/metrics',timeout=20)[1]
 val=req('POST','/v1/admin/validate',{},timeout=60)[1]
 files={f:os.path.getsize(os.path.join(D,f)) for f in os.listdir(D)}
 stop(p)
 p=start(False); restart=[]
 for i in [42,n//2,n-1]: restart.append((i,)+req('GET',f'/v1/nodes/{i}',timeout=20))
 met2=req('GET','/v1/metrics',timeout=20)[1]
 stop(p)
 res={'target_nodes':n,'workers':workers,'successful_inserts':len(l),'failures':len(fails),'total_seconds':total,'throughput_per_sec':len(l)/total,
      'insert_ms':{'p50':pct(l,50),'p95':pct(l,95),'p99':pct(l,99),'max':max(l) if l else None},
      'read_ms':{'p50':pct(samples['read'],50),'samples':samples['read']},'lattice_ms':{'p50':pct(samples['lattice'],50),'samples':samples['lattice']},'hybrid_ms':{'p50':pct(samples['hybrid'],50),'p95':pct(samples['hybrid'],95),'samples':samples['hybrid']},
      'metrics_before_restart':met,'metrics_after_restart':met2,'validate':val,'files':files,'restart_checks':[{'id':x[0],'status':x[1],'dt_ms':x[3],'content_prefix':x[2].get('content','')[:80]} for x in restart], 'first_failures':fails[:5]}
 open(E+f'/server_{n}_benchmark.json','w').write(json.dumps(res,indent=2))
 print(json.dumps(res,indent=2)[:4000])
 return res
if __name__=='__main__': bench(int(sys.argv[1]), int(sys.argv[2]))
