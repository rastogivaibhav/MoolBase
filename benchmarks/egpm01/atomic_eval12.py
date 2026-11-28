"""Development-only atomic extraction; gold documents used only in isolated oracle arm."""
import argparse,json,time
from pathlib import Path
from native_retrieval import prepare,bm25
from candidate_graph08 import select_source
from evidence_compiler11 import compile_edges
from candidate_eval08 import EXPECTED,NLI_REV
from independent_nli import Verifier
from quant_backend import serve
from quant_eval import obj,STR,structured

def main():
    p=argparse.ArgumentParser();p.add_argument('--offset',type=int,required=True);p.add_argument('--out',required=True);a=p.parse_args()
    assert 0<=a.offset<8
    rows,corpus,gold,manifest=prepare(120)
    row=rows[a.offset];q=row['question'];qid=row['id']
    models=json.loads(Path('egpm-models/manifest.json').read_text())
    assert all(models.get(k)==v for k,v in EXPECTED.items())
    verifier=Verifier();assert verifier.revision==NLI_REV
    proc=serve(models['executive_path'],18081,'egpm-models/executive.log')
    report=dict(id=qid,question=q,manifest=manifest,models=models,arms={},scope='development extraction only; no independent human precision or QA claim')
    path=Path(a.out);path.parent.mkdir(parents=True,exist_ok=True)
    def save():path.write_text(json.dumps(report,indent=2))
    def run(ids):
        source=select_source(ids,corpus,q);proposals=[];errors=[];calls=[]
        def call(prompt,schema,n):
            t=time.monotonic();result,err=structured(18081,prompt,schema,n)
            errors.extend(err);calls.append(dict(seconds=time.monotonic()-t,chars=len(prompt),cap=n,errors=err))
            return result or {}
        for sid,s in source.items():
            schema=obj({'links':{'type':'array','minItems':0,'maxItems':3,'items':obj({'subject':STR,'relation':{'type':'string','maxLength':100},'object':STR,'span':{'type':'string','enum':[sid]}})}})
            raw=call('Extract up to three explicit atomic factual relationships relevant to the question. Copy both entity names from the passage. Do not guess answers or infer missing links. Use a directed verb phrase. Question: '+q+'\nPassage '+sid+': '+s['text'],schema,384)
            proposals.extend(raw.get('links',[]))
        def critic(premise,claim):
            return call('Does the passage explicitly entail the exact directed claim? Refute only for explicit incompatibility; otherwise unresolved. Passage: '+premise+'\nClaim: '+claim,obj({'verdict':{'type':'string','enum':['support','refute','unresolved']}}),48).get('verdict','unresolved')
        accepted,audit=compile_edges(proposals,source,corpus,ids,verifier,critic)
        return dict(documents=ids,source=source,proposals=proposals,accepted=accepted,audit=audit,errors=errors,calls=calls,counts=dict(proposed=len(proposals),admitted=len(accepted),support=sum(x['verdict']=='support' for x in accepted),refute=sum(x['verdict']=='refute' for x in accepted)))
    try:
        report['arms']['retrieved']=run(bm25(corpus)(q)[:10]);save()
        report['arms']['oracle']=run(sorted(gold[qid]));save()
        print(json.dumps({k:v['counts'] for k,v in report['arms'].items()}),flush=True)
    finally:
        proc.terminate()
        try:proc.wait(timeout=10)
        except Exception:proc.kill();proc.wait()
if __name__=='__main__':main()
