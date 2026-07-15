import http.client,json,time,os,subprocess,shutil,threading,signal
PORT=19666; API='crash-key'; BIN='/mnt/data/gdb_do/build/graphenedb_server'; D='/tmp/gdb_crash_recovery'; E='/mnt/data/gdb_next_proof_evidence'; os.makedirs(E,exist_ok=True)
shutil.rmtree(D,ignore_errors=True); os.makedirs(D,exist_ok=True)
def req(m,path,body=None,timeout=10):
 c=http.client.HTTPConnection('127.0.0.1',PORT,timeout=timeout); h={'x-api-key':API}; data=None
 if body is not None: data=json.dumps(body,separators=(',',':')).encode(); h['Content-Type']='application/json'
 t=time.perf_counter(); c.request(m,path,data,h); r=c.getresponse(); raw=r.read(); c.close(); dt=(time.perf_counter()-t)*1000
 try: js=json.loads(raw.decode() or '{}')
 except: js={'raw':raw.decode(errors='replace')}
 return r.status,js,dt

def start():
 p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
 for _ in range(100):
  try:
   if req('GET','/v1/health',timeout=1)[0]==200: return p
  except Exception: time.sleep(.05)
 raise RuntimeError('no start')
p=start()
acks=[]; errs=[]; stop=False
def writer():
 i=0
 while not stop:
  try:
   s,js,dt=req('POST','/v1/nodes',{'text':f'crash recovery node {i} refund escalation causal chain','source':'crash'},timeout=10)
   if s==201: acks.append(js.get('id'))
   else: errs.append((s,js))
  except Exception as e: errs.append(('exception',str(e)))
  i+=1
th=threading.Thread(target=writer); th.start()
time.sleep(5)
# hard kill simulates process crash during active writes
p.kill(); p.wait(timeout=5); stop=True; th.join(timeout=5)
acked_before=len(acks); max_ack=max(acks) if acks else -1
files_after_kill={f:os.path.getsize(os.path.join(D,f)) for f in os.listdir(D)}
# restart and recover
p=start()
metrics=req('GET','/v1/metrics',timeout=30)[1]
validate=req('POST','/v1/admin/validate',{},timeout=120)
checks=[]
for i in [0, max(0,acked_before//2), max_ack]:
 checks.append({'id':i,'result':req('GET',f'/v1/nodes/{i}',timeout=30)})
lattice=req('GET','/v1/search/lattice?node_id=0&hops=2',timeout=30)
files_after_restart={f:os.path.getsize(os.path.join(D,f)) for f in os.listdir(D)}
p.terminate(); p.wait(timeout=10)
res={'acked_before_kill':acked_before,'max_ack_id':max_ack,'write_errors_observed':len(errs),'first_errors':errs[:5],'files_after_kill':files_after_kill,'metrics_after_restart':metrics,'validate_after_restart':{'status':validate[0],'dt_ms':validate[2],'body':validate[1]},'read_checks':checks,'lattice_after_restart':{'status':lattice[0],'dt_ms':lattice[2],'body_prefix':str(lattice[1])[:500]},'files_after_restart':files_after_restart}
open(E+'/crash_recovery_active_writes.json','w').write(json.dumps(res,indent=2))
print(json.dumps(res,indent=2)[:5000])
