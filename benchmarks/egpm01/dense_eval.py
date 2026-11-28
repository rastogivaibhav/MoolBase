"""Same encoder, exact cosine baseline and native MoolBase Flat index."""
import argparse,json,subprocess,tempfile
from pathlib import Path
import numpy as np
import torch
from transformers import AutoModel,AutoTokenizer
from huggingface_hub import model_info
from native_retrieval import prepare,bm25
from hybrid_gate import fused_ranks
MODEL="sentence-transformers/all-MiniLM-L6-v2"
def embed(texts,model,tok):
    chunks=[]
    for i in range(0,len(texts),32):
        t=tok(texts[i:i+32],padding=True,truncation=True,max_length=256,return_tensors="pt")
        with torch.inference_mode():
            h=model(**t).last_hidden_state;mask=t["attention_mask"].unsqueeze(-1);v=(h*mask).sum(1)/mask.sum(1).clamp(min=1)
            chunks.append(torch.nn.functional.normalize(v,p=2,dim=1).numpy())
    return np.concatenate(chunks)
def main():
    p=argparse.ArgumentParser();p.add_argument("--binary",required=True);p.add_argument("--rows",type=int,default=120);p.add_argument("--out",required=True);p.add_argument("--require-full",action="store_true");a=p.parse_args()
    torch.set_num_threads(4);torch.manual_seed(0)
    selected,corpus,gold,manifest=prepare(a.rows)
    if a.require_full and (len(selected)!=1000 or len({r["id"] for r in selected})!=1000 or manifest["manifest_size"]!=1000): raise ValueError("incomplete author selection")
    docids=sorted(corpus);questions=[r["question"] for r in selected];k=min(20,len(docids))
    revision="1110a243fdf4706b3f48f1d95db1a4f5529b4d41";tok=AutoTokenizer.from_pretrained(MODEL,revision=revision);model=AutoModel.from_pretrained(MODEL,revision=revision).eval()
    D=embed([corpus[d] for d in docids],model,tok);Q=embed(questions,model,tok)
    with tempfile.TemporaryDirectory() as tmp:
        f=Path(tmp)/"vectors.txt"
        with f.open("w") as h:
            h.write(f"{len(D)} {len(Q)} {D.shape[1]} {k}\n");np.savetxt(h,D,fmt="%.9g");np.savetxt(h,Q,fmt="%.9g")
        result=subprocess.run([str(Path(a.binary).resolve()),str(Path(tmp)/"db"),str(f)],check=True,capture_output=True,text=True)
    native=[[docids[int(i)] for i in line.split()] for line in result.stdout.strip().splitlines()]
    assert len(native)==len(Q) and all(len(r)==k and len(set(r))==k for r in native)
    scores=Q@D.T;rank=bm25(corpus);records=[]
    for i,row in enumerate(selected):
        d=[docids[j] for j in np.argsort(-scores[i],kind="stable")[:k]]
        b=rank(row["question"])[:k];n=native[i];records.append(dict(id=row["id"],gold=sorted(gold[row["id"]]),bm25=b,dense_exact=d,moolbase_dense=n,rrf=fused_ranks([b,n],k)))
    arms=["bm25","dense_exact","moolbase_dense","rrf"]
    metrics={arm:{str(k):{"recall":sum(len(set(r[arm][:k])&set(r["gold"]))/len(r["gold"]) for r in records)/len(records),"complete":sum(set(r["gold"]).issubset(r[arm][:k]) for r in records)} for k in [5,10,20]} for arm in arms}
    parity={str(k):sum(len(set(r["dense_exact"][:k])&set(r["moolbase_dense"][:k]))/k for r in records)/len(records) for k in [5,10,20]}
    report=dict(scope="retrieval diagnostic, not answer accuracy",model=MODEL,revision=revision,docs=len(docids),questions=len(records),manifest=manifest,metrics=metrics,parity=parity,records=records)
    Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!="records"},indent=2),flush=True)
if __name__=="__main__":main()
