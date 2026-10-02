#!/usr/bin/env python3
"""Start a local pilot server, ingest evidence, reason and verify file restart."""
import argparse,json,os,secrets,signal,socket,subprocess,sys,tempfile,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'clients/python'))
from graphenedb_client import GrapheneDBClient,GrapheneDBError

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,default=Path('build-showcase/graphenedb_server'));p.add_argument('--output',type=Path,default=Path('reports/customer-showcase/python-http.json'));a=p.parse_args()
 binary=a.server.resolve();key=secrets.token_urlsafe(24)
 with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
 client=GrapheneDBClient(f'http://127.0.0.1:{port}',api_key=key)
 with tempfile.TemporaryDirectory(prefix='moolbase-http-') as temp:
  db=Path(temp)/'db';db.mkdir();log=open(Path(temp)/'server.log','w+')
  def start():
   process=subprocess.Popen([str(binary),str(db),'16',str(port),'--bind-address','127.0.0.1'],env={**os.environ,'GRAPHENEDB_API_KEY':key},stdout=subprocess.DEVNULL,stderr=log)
   for _ in range(200):
    if process.poll() is not None:log.seek(0);raise RuntimeError(log.read())
    try:
     if client.ready().status==200:return process
    except GrapheneDBError:pass
    time.sleep(.025)
   process.kill();process.wait();raise RuntimeError('Server readiness timed out')
  def stop(process):
   process.send_signal(signal.SIGTERM)
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait();raise RuntimeError('Server shutdown timed out')
   if process.returncode:raise RuntimeError(f'Server exit {process.returncode}')
  def reason():
   # The current client has no public reason_runtime method. Use the documented
   # HTTP endpoint directly, rather than confusing hypothesis proposals with a decision.
   import urllib.request
   payload={'query':'checkout failures increased','signature':33,'mode':'empirical','minimum_confidence':.3,'max_hops':5,'max_paths':32}
   request=urllib.request.Request(client.base_url+'/v1/reason/runtime',data=json.dumps(payload).encode(),headers={'X-API-Key':key,'Content-Type':'application/json'},method='POST')
   with urllib.request.urlopen(request,timeout=10) as response:return json.load(response)
  extraction={'schema_version':1,'source_id':'onboarding-incident','signature':33,'nodes':[{'external_id':'release','content':'release changed pool timeout','role':'root'},{'external_id':'pool','content':'connection pool exhausted','role':'node'},{'external_id':'checkout','content':'checkout failures increased','role':'symptom'}],'relations':[{'from_external_id':'release','to_external_id':'pool','origin':'observed','role':'mechanistic','confidence':.97,'evidence_id':'config-diff'},{'from_external_id':'pool','to_external_id':'checkout','origin':'discovered','role':'causal','confidence':.96,'evidence_id':'pool-trace'}]}
  process=start()
  try:
   imported=client.put_extraction(extraction);first=reason();replayed=client.put_extraction(extraction)
   if imported.status!=201 or replayed.status!=200:raise RuntimeError('Extraction replay contract failed')
   if not first['receipt']['graphene_executed']:raise RuntimeError('Core did not execute')
  finally:stop(process)
  process=start()
  try:
   after=reason()
   if first['receipt']['final_bundle_hash']!=after['receipt']['final_bundle_hash']:raise RuntimeError('Process restart changed evidence bundle')
  finally:stop(process);log.close()
  a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'beforeRestart':first,'afterRestart':after,'scope':'stateless HTTP reasoning; no lifecycle supersession or persisted prior reasoning state'},indent=2)+'\n')
  print(f'HTTP ingestion and retry-safe replay: PASS; native server process restart: PASS')
  print(f'Decision status: {first["status"]}; inspect status and uncertainty before acting.');print(f'Receipt: {a.output}')
if __name__=='__main__':main()
