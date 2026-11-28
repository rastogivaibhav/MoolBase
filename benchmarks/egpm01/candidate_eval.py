"""EGPM07 development only: candidate proof search, matched text and verifier controls."""
import argparse,json,time
from pathlib import Path
from native_retrieval import prepare,bm25
from quant_eval import obj,STR,GROUND,PIN,FINAL,structured,ctx,score
from quant_backend import serve
from independent_nli import Verifier,MODEL as NLI_MODEL
from hybrid_gate import fused_ranks,native_gate,validate_candidate
from candidate_graph import select_source,grounded_candidates,evaluate_proof,resolve,audit_sources
from graph_adapter import normalize
VERDICT={"type":"string","enum":["support","refute","unresolved"]}
CRITIC=obj({"first":VERDICT,"second":VERDICT,"question_coverage":VERDICT})
IDENTITY=obj({"verdict":VERDICT})
CAND=obj({"answers":{"type":"array","minItems":0,"maxItems":3,"items":STR}})
EXPECTED={"executive_revision":"7dabda4d13d513e3e842b20f0d435c732f172cbe","executive_sha256":"626b4a6678b86442240e33df819e00132d3ba7dddfe1cdc4fbb18e0a9615c62d","memory_revision":"48bc49c6d0dd992fa22d96de14b99c11351d6392","memory_quantized_sha256":"6618248d7af50237d155b95c2ec7b7b89ba583dee34570cc826dcafcc9eaf514"}
NLI_REV="fa2804872c3b4bd748f38c0185cc85775361e735"
def schema(source):
    return obj({"links":{"type":"array","minItems":0,"maxItems":2,"items":obj({"subject":STR,"relation":{"type":"string","maxLength":100},"object":STR,"span":{"type":"string","enum":list(source)}})}})
def render(source):
    return '\n'.join(k+': '+v['text'] for k,v in source.items())
def proof_search(q,ids,corpus,get,verify,binary):
    source=select_source(ids,corpus,q)
    audit_sources(source,corpus,ids)
    candidates=grounded_candidates(get("Suggest up to three distinct short possible answers to this question, drawn literally from the passages. Do not invent a candidate or combine alternatives into one answer. Return an empty list if none.\nQuestion: "+q+"\nPassages:\n"+render(source),CAND,128),source)
    proofs=[];all_source=dict(source)
    def critic(q,c,links,source):
        body=[{"claim":l["claim"],"evidence":source[l["span"]]["text"]} for l in links]
        return get("Independently review the proposed proof. For each claim choose support only if the cited text entails that exact relation in that direction between those entities; refute only for explicit incompatibility; otherwise unresolved. Mere co-occurrence, a different role, financing vs distribution, and featuring vs being the main artist are not interchangeable. For question_coverage choose support only if the two claims together establish every constraint of the exact question and candidate, without missing links. Do not use external knowledge. If no second claim choose unresolved for second and question_coverage.\nQuestion: "+q+"\nCandidate: "+c+"\nClaims and evidence: "+json.dumps(body,ensure_ascii=False),CRITIC,96) or {}
    def identity(a,b,t):
        r=get("Does this text explicitly identify these two names as the same entity? A shared substring, different office or co-occurrence is not identity. Use support, refute or unresolved.\nNames: "+a+" | "+b+"\nText: "+t,IDENTITY,32)
        return r["verdict"] if r else "unresolved"
    for c in candidates:
        src=select_source(ids,corpus,q,c);audit_sources(src,corpus,ids);all_source.update(src)
        raw=get("Find a two-link directed evidence proof for this possible answer, starting at an explicitly named entity in the question, through a shared intermediate entity, ending at the candidate. Select existing evidence IDs and copy entity names as written in their source windows. Use the relation that the question actually asks, expressed as a short verb phrase connecting subject to object. Do not infer unsupported relations or use background knowledge. If only the first link is supported, return one link; if none, return empty. Do not manufacture the second link.\nQuestion: "+q+"\nCandidate: "+c+"\nEvidence:\n"+render(src),schema(src),384)
        proof=evaluate_proof(q,c,raw,src,verify,critic,identity,binary);proof['raw']=raw;proofs.append(proof)
    return {"retrieved":list(ids),"spans":all_source,"proposals":candidates,"proofs":proofs,**resolve(proofs),"verifier_control":resolve(proofs,True)}
def active(q,initial,static,corpus,rank,get,verify,binary):
    if static['status']=='resolved':return {**static,"new_docs":[],"retrieval_queries":[]}
    queries=[q+' '+p for r in static['proofs'] for p in r['prefix']]
    queries=list(dict.fromkeys(queries))[:3] or [q+' '+' '.join(static['proposals'])]
    ranked=fused_ranks([rank(s)[:20] for s in queries],20)
    new=[d for d in ranked if d not in initial][:6]
    if not new:return {**static,"new_docs":[],"retrieval_queries":queries}
    revised=proof_search(q,initial+new,corpus,get,verify,binary)
    # Preserve old hypotheses/evidence alongside the new round, not silent overwrite.
    proofs=static['proofs']+revised['proofs']
    return {**revised,"proofs":proofs,"spans":{**static['spans'],**revised['spans']},"proposals":list(dict.fromkeys(static['proposals']+revised['proposals'])),"new_docs":new,"retrieval_queries":queries,**resolve(proofs),"verifier_control":resolve(proofs,True)}
def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--graph',required=True);p.add_argument('--offset',type=int,required=True);p.add_argument('--out',required=True);a=p.parse_args()
    if not 0<=a.offset<8:raise ValueError('EGPM07 is restricted to the eight development questions')
    rows,corpus,gold,manifest=prepare(120);row=rows[a.offset];qid=row['id'];q=row['question'];rank=bm25(corpus)
    if not qid.startswith('2hop__'):raise ValueError('two-hop development protocol only')
    models=json.loads(Path('egpm-models/manifest.json').read_text())
    if any(models.get(k)!=v for k,v in EXPECTED.items()):raise RuntimeError('model identity drift')
    verifier=Verifier()
    if verifier.revision!=NLI_REV:raise RuntimeError('NLI revision drift')
    ep,mp=18081,18082;e=serve(models['executive_path'],ep,'egpm-models/executive.log');m=None
    try:
        m=serve(models['memory_path'],mp,'egpm-models/memory.log');start=time.monotonic();errors=[];trace=[];calls=[]
        def call(port,prompt,schema,n=192):
            t=time.monotonic();r,err=structured(port,prompt,schema,n);errors.extend(err);calls.append(dict(model='executive' if port==ep else 'memo',prompt_chars=len(prompt),output_cap=n,seconds=time.monotonic()-t,errors=err));return r
        def get(prompt,schema,n=192):return call(ep,prompt,schema,n)
        def short(ids):
            r=get("Give the shortest answer span. If evidence is insufficient say No answer.\nQuestion: "+q+"\nPassages:\n"+ctx(ids,corpus),FINAL,64);return r['answer'] if r else 'No answer'
        b=rank(q)[:10];pred={'closed_book':short([]),'A_rag':short(b)}
        # Same MeMo inference prompts as EGPM06, no gold in ordinary arms.
        g=get("Split the question into one or two atomic factual questions.\n"+q,GROUND,192)
        def recall(s):
            r=call(mp,"Answer this question from memory. If uncertain say I don't know.\n"+s,FINAL,96);trace.append({'question':s,'answer':r['answer'] if r else "I don't know"})
        for s in (g or {}).get('questions',[])[:2]:recall(s)
        pin=get("Identify an intermediate entity and ask one follow-up question.\nOriginal: "+q+"\nReplies: "+json.dumps(trace),PIN,192)
        if pin and pin['question']:recall(pin['question'])
        f=get("Synthesize the shortest answer from memory. If uncertain say No answer.\nQuestion: "+q+"\nMemory: "+json.dumps(trace),FINAL,64);pred['B_memo']=f['answer'] if f else 'No answer'
        aug=fused_ranks([rank(q)[:20],rank(q+' '+json.dumps(trace)+' '+pred['B_memo'])[:20]],10)
        arms={}
        for name,ids in [('C',b),('D',aug)]:
            t=time.monotonic();s=proof_search(q,ids,corpus,get,verifier,a.graph);s['seconds']=time.monotonic()-t;arms[name+'_static']=s
            t=time.monotonic();v=active(q,ids,s,corpus,rank,get,verifier,a.graph);v['seconds']=time.monotonic()-t;arms[name+'_active']=v
        for name,arm in arms.items():
            audit_sources(arm['spans'],corpus,arm['retrieved'])
            pred[name+'_graph']=arm['answer'];pred[name+'_matched_text']=short(arm['retrieved']);pred[name+'_verifier_only']=arm['verifier_control']['answer']
            result={'answer':'No answer','status':'not_attempted','receipt':None}
            if arm['status']=='resolved':
                proof=next(p for p in arm['proofs'] if p['status']=='supported' and normalize(p['candidate'])==normalize(arm['answer']))
                picked=[arm['spans'][l['span']] for l in proof['links']]
                candidate=validate_candidate({'answer':arm['answer'],'citations':[{'id':v['doc'],'quote':v['text']} for v in picked]},corpus)
                result=native_gate(a.binary,candidate,'support')
            arm['epistemic']=result;pred[name+'_epistemic']=result['answer']
        # Gold documents/labels only after ordinary inference. Oracle uses identical proof extraction.
        oi=sorted(gold[qid]);pred['oracle_text']=short(oi);o=proof_search(q,oi,corpus,get,verifier,a.graph);arms['oracle_graph']=o;pred['oracle_graph']=o['answer']
        aliases=[row['answer']]+row.get('answer_aliases',[])
        report=dict(scope='EGPM07 two-hop development only; candidate proof search; dual verification; matched documents not matched compute; exact/F1 not paper judge',id=qid,question=q,aliases=aliases,manifest=manifest,models={k:v for k,v in models.items() if not k.endswith('_path')},nli_model=NLI_MODEL,nli_revision=verifier.revision,trace=trace,predictions=pred,scores={k:score(v,aliases) for k,v in pred.items()},arms=arms,errors=errors,calls=calls,seconds=time.monotonic()-start)
        Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2))
        print(json.dumps({**{k:report[k] for k in ('id','predictions','scores','errors','seconds')},"diagnostics":{k:dict(status=v['status'],candidates=v['proposals'],proofs=len(v['proofs']),supported_links=sum(l['verdict']=='support' for p in v['proofs'] for l in p['links']),complete_paths=sum(p['status']=='supported' for p in v['proofs']),errors=[x for p in v['proofs'] for x in p['errors']],new_docs=len(v.get('new_docs',[]))) for k,v in arms.items()}},indent=2),flush=True)
    finally:
        for proc in (m,e):
            if proc:
                proc.terminate()
                try:proc.wait(timeout=5)
                except Exception:proc.kill();proc.wait()
if __name__=='__main__':main()
