"""Bounded structured MeMo-style CPU pilot with constrained decoding and matched controls.
This is an adapted protocol (not the paper's exact prompts or judge).
"""
import argparse,json,time,re
from pathlib import Path
from collections import Counter
from native_retrieval import prepare,bm25
from hybrid_gate import fused_ranks,native_gate,validate_candidate
from quant_backend import ask,serve
def obj(props,required=None):return {"type":"object","properties":props,"required":required or list(props),"additionalProperties":False}
STR={"type":"string","maxLength":240}
QUEST={"type":"array","items":STR,"minItems":1,"maxItems":2}
GROUND=obj({"questions":QUEST})
PIN=obj({"entity":STR,"question":STR})
FINAL=obj({"answer":STR})
CAND=obj({"answer":STR,"citations":{"type":"array","minItems":0,"maxItems":2,"items":obj({"id":STR,"quote":{"type":"string","maxLength":300}})}})
VERDICT=obj({"verdict":{"type":"string","enum":["support","refute","unresolved"]}})
def structured(port,prompt,schema,n=192):
    errors=[]
    for attempt in range(2):
        try:return json.loads(ask(port,prompt+("\nBe concise. Return only valid JSON." if attempt else ""),n if not attempt else n+96,schema)),errors
        except (ValueError,KeyError) as e:errors.append(str(e))
    return None,errors
def norm(s):return " ".join(re.findall(r"\w+",s.casefold()))
def score(pred,aliases):
    p=norm(pred);a=[norm(x) for x in aliases];t=p.split()
    def f1(x):
        u=x.split();o=sum((Counter(t)&Counter(u)).values())
        return 2*o/(len(t)+len(u)) if t and u else 0
    return {"exact":bool(p) and p in a,"f1":max([f1(x) for x in a],default=0)}
def ctx(ids,corpus):return "\n\n".join("["+d+"] "+corpus[d] for d in ids)
def main():
    p=argparse.ArgumentParser();p.add_argument("--binary",required=True);p.add_argument("--offset",type=int,required=True);p.add_argument("--out",required=True);a=p.parse_args()
    rows,corpus,gold,manifest=prepare(120);rank=bm25(corpus)
    models=json.loads(Path("egpm-models/manifest.json").read_text());ep,mp=18081,18082
    e=serve(models["executive_path"],ep,"egpm-models/executive.log");m=None
    try:
        m=serve(models["memory_path"],mp,"egpm-models/memory.log")
        row=rows[a.offset];q=row["question"];qid=row["id"];start=time.monotonic();errors=[];trace=[]
        def get(port,prompt,schema,n=192):
            result,err=structured(port,prompt,schema,n);errors.extend(err);return result
        def short_answer(ids):
            r=get(ep,"Give the shortest answer span to the question. If evidence is insufficient, use No answer.\nQuestion: "+q+"\nPassages:\n"+ctx(ids,corpus),FINAL,64)
            return r["answer"] if r else "No answer"
        b=rank(q)[:5];pred={"closed_book":short_answer([]),"A_rag":short_answer(b),"oracle":short_answer(sorted(gold[qid]))}
        # Grounding: no benchmark answer, decomposition, evidence IDs or source text.
        g=get(ep,"Split this question into one or two atomic factual subquestions to identify the intermediate entity. Return questions only.\n"+q,GROUND,192)
        def recall(question):
            r=get(mp,"Answer this question from your memory. If uncertain say I don't know.\n"+question,FINAL,96)
            trace.append({"question":question,"answer":r["answer"] if r else "I don't know"})
        for sub in (g or {}).get("questions",[])[:2]:recall(sub)
        pin=get(ep,"Given memory replies, identify the intermediate entity and ask one targeted follow-up question about it. If unknown, ask another question.\nOriginal: "+q+"\nReplies: "+json.dumps(trace),PIN,192)
        if pin and pin["question"]:recall(pin["question"])
        final=get(ep,"Synthesize the shortest final answer using these memory responses. If conflicting or unknown, say No answer.\nQuestion: "+q+"\nMemory: "+json.dumps(trace),FINAL,64)
        pred["B_memo"]=final["answer"] if final else "No answer"
        hint=json.dumps(trace);aug=fused_ranks([rank(q)[:20],rank(q+" "+hint+" "+pred["B_memo"])[:20]],5)
        gates={}
        for name,ids,recalls in [("C",b,""),("D",aug,hint)]:
            docs={d:corpus[d] for d in ids}
            raw=get(ep,"Answer with a short span and one or two exact verbatim quotations from the provided passages with their IDs. Recall is not evidence. If insufficient, return empty answer and citations.\nQuestion: "+q+"\nRecall: "+recalls+"\nPassages:\n"+ctx(ids,corpus),CAND,384)
            c=None;v="unresolved"
            try:c=validate_candidate(raw,docs)
            except (ValueError,TypeError,KeyError) as ex:errors.append(str(ex))
            if c:
                quoted="\n".join(x["quote"] for x in c["citations"])
                vv=get(ep,"Do the quotes establish every required link between question and answer? Choose support only if all links are present, refute only for direct contradiction, otherwise unresolved.\nQuestion: "+q+"\nAnswer: "+c["answer"]+"\nQuotes: "+quoted,VERDICT,32)
                if vv:v=vv["verdict"]
            pred[name+"_verifier_only"]=c["answer"] if c and v=="support" else "No answer"
            result=native_gate(a.binary,c,v) if c else {"answer":"No answer","status":"invalid_candidate","receipt":None}
            pred[name+"_moolbase"]=result["answer"];gates[name]={"candidate":c,"verdict":v,"native":result,"retrieved":ids}
        aliases=[row["answer"]]+row.get("answer_aliases",[])
        report={"scope":"Q4_K_M CPU, adapted bounded structured MeMo-style protocol, same-executive verification; exact/F1 not paper judge","id":qid,"question":q,"aliases":aliases,"models":{k:v for k,v in models.items() if not k.endswith("_path")},"manifest":manifest,"predictions":pred,"scores":{k:score(v,aliases) for k,v in pred.items()},"trace":trace,"gates":gates,"errors":errors,"seconds":time.monotonic()-start}
        Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ("id","predictions","scores","errors","seconds")},indent=2),flush=True)
    finally:
        for proc in [m,e]:
            if proc:
                proc.terminate()
                try:proc.wait(timeout=5)
                except Exception:proc.kill();proc.wait()
if __name__=="__main__":main()
