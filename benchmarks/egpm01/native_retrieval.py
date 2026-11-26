#!/usr/bin/env python3
"""EGPM-01: real MuSiQue retrieval only; no gold labels passed to retrieval."""
import argparse, hashlib, json, math, os, re, secrets, socket, subprocess, sys, tempfile, time, urllib.request, urllib.parse
from collections import Counter
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"clients/python"))
from graphenedb_client import GrapheneDBClient, GrapheneDBError
MANIFEST="https://raw.githubusercontent.com/arunv3rma/MeMo/main/data_synthesis_pipeline/data_subsets/musique_negative_N_doc_ids_per_query.json"
DATASET="awinml/musique"
ROWS="https://datasets-server.huggingface.co/rows?"
STOP=set("a an the of in on to is was were are and or for by with from what which who whose where when how did does do has have had that this".split())
def tokens(s): return [t for t in re.findall("[a-z0-9]+",s.lower()) if t not in STOP]
def load(url):
    for i in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url,headers={"User-Agent":"EGPM-01/1.0"}),timeout=90) as r: raw=r.read()
            return json.loads(raw),hashlib.sha256(raw).hexdigest()
        except Exception:
            if i==3: raise
            time.sleep(2**i)
def prepare(n):
    manifest,mhash=load(MANIFEST)
    rows=[]; hashes={}; offset=0
    while offset<n:
        url=ROWS+urllib.parse.urlencode(dict(dataset=DATASET,config="default",split="validation",offset=offset,length=min(20,n-offset)))
        page,sha=load(url); hashes[str(offset)]=sha
        batch=[r["row"] for r in page["rows"]]
        if not batch: break
        rows.extend(batch);offset+=len(batch)
    selected=[r for r in rows if r["id"] in manifest and r["answerable"]]
    corpus={}; gold={}
    for row in selected:
        qid=row["id"];gold[qid]=set()
        for p in row["paragraphs"]:
            did=f"{qid}_para{p['idx']}_chunk0"
            if p["is_supporting"]: gold[qid].add(did)
            if p["is_supporting"] or did in manifest[qid]:
                corpus[did]=p["title"]+" "+p["paragraph_text"]
        if not gold[qid] or not set(manifest[qid]).issubset(corpus): raise ValueError("incomplete selected corpus")
    if not selected: raise ValueError("no MeMo IDs matched")
    return selected,corpus,gold,dict(manifest_sha256=mhash,rows_sha256=hashes,source_rows=len(rows),manifest_size=len(manifest))
def bm25(corpus):
    counts={d:Counter(tokens(s)) for d,s in corpus.items()}; df=Counter(t for c in counts.values() for t in c)
    sizes={d:sum(c.values()) for d,c in counts.items()};avg=sum(sizes.values())/len(sizes)
    def rank(q):
        ts=set(tokens(q))
        def score(d):
            return sum(math.log(1+(len(corpus)-df[t]+.5)/(df[t]+.5))*counts[d][t]*2.2/(counts[d][t]+1.2*(.25+.75*sizes[d]/avg)) for t in ts)
        return sorted(corpus,key=lambda d:(-score(d),d))
    return rank
def metrics(records):
    return {arm:{str(k):{"macro_recall":sum(len(set(r[arm][:k])&set(r["gold"]))/len(r["gold"]) for r in records)/len(records),"all_evidence_count":sum(set(r["gold"]).issubset(r[arm][:k]) for r in records)} for k in (5,10,20)} for arm in ("bm25","moolbase_vector")}
def main():
    p=argparse.ArgumentParser();p.add_argument("--server",required=True);p.add_argument("--rows",type=int,default=40);p.add_argument("--out",required=True);a=p.parse_args()
    selected,corpus,gold,manifest=prepare(a.rows)
    print(f"Matched {len(selected)} questions, {len(corpus)} documents",flush=True)
    with tempfile.TemporaryDirectory() as td:
        db=Path(td)/"db";db.mkdir()
        with socket.socket() as sock: sock.bind(("127.0.0.1",0));port=sock.getsockname()[1]
        key=secrets.token_urlsafe(24)
        with open(Path(td)/"server.log","w+") as log:
            proc=subprocess.Popen([str(Path(a.server).resolve()),str(db),"128",str(port),"--bind-address","127.0.0.1","--rate-limit-rps","10000","--rate-limit-burst","10000"],env={**os.environ,"GRAPHENEDB_API_KEY":key},stdout=subprocess.DEVNULL,stderr=log)
            try:
                client=GrapheneDBClient(f"http://127.0.0.1:{port}",api_key=key)
                for _ in range(200):
                    if proc.poll() is not None: raise RuntimeError("server exited")
                    try:
                        if client.ready().status==200: break
                    except GrapheneDBError: pass
                    time.sleep(.05)
                else: raise RuntimeError("server not ready")
                ids={}
                for did,s in corpus.items():
                    response=client.put_node(s,source=did,idempotency_key=did)
                    assert response.status==201
                    ids[response.data["id"]]=did
                assert len(ids)==len(corpus)
                rank=bm25(corpus);records=[]
                for row in selected:
                    qid=row["id"];q=row["question"]
                    hits=client.search(q,top_k=20).data["results"]
                    native=[ids[h["id"]] for h in hits]
                    assert len(native)==len(set(native)) and len(native)==min(20,len(corpus))
                    records.append(dict(id=qid,gold=sorted(gold[qid]),bm25=rank(q)[:20],moolbase_vector=native))
                report=dict(scope="retrieval diagnostic; not answer accuracy or epistemic reasoning",documents=len(corpus),questions=len(records),hops=dict(Counter(r["id"].split("__")[0] for r in records)),manifest=manifest,metrics=metrics(records),records=records)
                Path(a.out).parent.mkdir(parents=True,exist_ok=True)
                Path(a.out).write_text(json.dumps(report,indent=2)+"\n")
                print(json.dumps({k:v for k,v in report.items() if k!="records"},indent=2),flush=True)
            except Exception:
                log.seek(0);print(log.read()[-6000:],file=sys.stderr);raise
            finally:
                proc.terminate()
                try: proc.wait(timeout=10)
                except subprocess.TimeoutExpired: proc.kill();proc.wait()
if __name__=="__main__": main()
