#!/usr/bin/env python3
import json, os, sys, time, urllib.request
base=os.environ.get('GDB_URL','http://127.0.0.1:18080')
key=os.environ.get('GRAPHENEDB_API_KEY','launch-test-key')
def call(method,path,payload=None, auth=True):
    data=None if payload is None else json.dumps(payload).encode()
    headers={'Content-Type':'application/json'}
    if auth: headers['X-API-Key']=key
    req=urllib.request.Request(base+path,data=data,headers=headers,method=method)
    with urllib.request.urlopen(req,timeout=30) as r: return r.status,json.loads(r.read())
for _ in range(60):
    try:
        if call('GET','/v1/health',auth=False)[0]==200: break
    except Exception: time.sleep(.25)
else: raise SystemExit('server did not become healthy')
assert call('GET','/v1/ready')[1]['status']=='ready'
ids=[]
for text in ['checkout deployment','memory leak observed','service outage','traffic spike alternative']:
    st,res=call('POST','/v1/nodes',{'text':text,'source':'launch-smoke'})
    assert st==201, (st,res); ids.append(res['id'])
assert call('GET',f'/v1/nodes/{ids[1]}')[1]['content']=='memory leak observed'
assert call('GET',f'/v1/search/lattice?node_id={ids[1]}')[0]==200
assert call('POST','/v1/search/hybrid',{'query':'why did checkout fail','top_k':3})[0]==200
assert call('POST','/v1/admin/validate',{})[1]['ok'] is True
assert call('POST','/v1/admin/checkpoint',{})[1]['ok'] is True
print(json.dumps({'ok':True,'ids':ids,'ready':call('GET','/v1/ready')[1]}))
