import subprocess, time, http.client, json, os, shutil, signal
BIN='/mnt/data/gdb_do/build/graphenedb_server'; D='/tmp/gdb_quick'; PORT=19222; API='k'
shutil.rmtree(D, ignore_errors=True); os.makedirs(D)
p=subprocess.Popen([BIN,D,'64',str(PORT),'--physical-lattice-primary','--api-key',API],stderr=subprocess.PIPE,text=True)
time.sleep(.5)
def req(m,pth,b=None):
 c=http.client.HTTPConnection('127.0.0.1',PORT,timeout=5); h={'x-api-key':API}; data=None
 if b is not None: data=json.dumps(b).encode(); h['Content-Type']='application/json'
 t=time.perf_counter(); c.request(m,pth,data,h); r=c.getresponse(); raw=r.read(); c.close(); return r.status,raw,(time.perf_counter()-t)*1000
l=[]
for i in range(1000):
 s,raw,dt=req('POST','/v1/nodes',{'text':f'node {i}'})
 if s!=201: print('fail',i,s,raw[:100]); break
 l.append(dt)
 if i%100==0: print(i,dt)
print('done',len(l),'avg',sum(l)/len(l),'max',max(l))
print(req('GET','/v1/metrics')[1])
p.terminate(); p.wait(); print('stderr',p.stderr.read()[-500:])
