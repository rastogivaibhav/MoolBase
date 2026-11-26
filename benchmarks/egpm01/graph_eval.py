"""Bounded structured MeMo-style CPU pilot with constrained decoding and matched controls.
This is an adapted protocol (not the paper's exact prompts or judge).
"""
import argparse,json,time,re
from pathlib import Path
from collections import Counter
from native_retrieval import prepare,bm25
from hybrid_gate import fused_ranks,native_gate,validate_candidate
from graph_adapter import spans,materialize,python_paths,native_paths,decide
from independent_nli import Verifier,MODEL as NLI_MODEL
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
    p=argparse.ArgumentParser();p.add_argument("--binary",required=True);p.add_argument("--graph",required=True);p.add_argument("--offset",type=int,required=True);p.add_argument("--out",required=True);a=p.parse_args()
    rows,corpus,gold,manifest=prepare(120);rank=bm25(corpus)
    models=json.loads(Path("egpm-models/manifest.json").read_text());ep,mp=18081,18082
    verifier=Verifier()
    e=serve(models["executive_path"],ep,"egpm-models/executive.log");m=None
    try:
        m=serve(models["memory_path"],mp,"egpm-models/memory.log")
        row=rows[a.offset];q=row["question"];qid=row["id"];start=time.monotonic();errors=[];trace=[]
        def get(port,prompt,schema,n=192):
            result,err=structured(port,prompt,schema,n);errors.extend(err);return result
        def short_answer(ids):
            r=get(ep,"Give the shortest answer span to the question. If evidence is insufficient, use No answer.\nQuestion: "+q+"\nPassages:\n"+ctx(ids,corpus),FINAL,64)
            return r["answer"] if r else "No answer"
        b=rank(q)[:10];pred={"closed_book":short_answer([]),"A_rag":short_answer(b),"oracle":short_answer(sorted(gold[qid]))}
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
        hint=json.dumps(trace);aug=fused_ranks([rank(q)[:20],rank(q+" "+hint+" "+pred["B_memo"])[:20]],10)
        # One common question plan, generated without retrieved answers or gold.
        PLAN=obj({"anchor":STR,"relations":{"type":"array","items":{"type":"string","maxLength":100},"minItems":2,"maxItems":4}})
        TRIPLE=obj({"triples":{"type":"array","minItems":0,"maxItems":8,"items":obj({"stage":{"type":"integer","minimum":0,"maximum":3},"subject":STR,"object":STR,"span":STR})}})
        plan=get(ep,"Translate the question into a directed chain from an explicitly named starting entity in the question to the unknown final answer. Return anchor (exact entity name appearing in question) and ordered verb phrases, each grammatically connecting subject to object. Example: 'Who founded the company that distributed UHF?' anchor UHF, relations ['was distributed by','was founded by']. Do not use the example's values for other questions. Do not guess missing entities.\nQuestion: "+q,PLAN,192)
        gates={}
        for name,ids,recalls in [("C",b,""),("D",aug,hint)]:
            source=spans(ids,corpus);rows=[]
            if plan and plan["anchor"].casefold() in q.casefold():
                keys=list(source)
                for i in range(0,len(keys),16):
                    chunk=keys[i:i+16]
                    text="\n".join("["+k+"] "+source[k]["text"] for k in chunk)
                    raw=get(ep,"Extract only relations explicitly stated in these sentences that match one of the ordered relation phrases. Return stage (zero-based), subject and object exactly as written in the same sentence, and the sentence ID in span. Include explicitly negated relations too, without changing the relation phrase. No inferred claims, no guesses.\nQuestion: "+q+"\nPlan: "+json.dumps(plan)+"\nSentences:\n"+text,TRIPLE,512)
                    rows.extend((raw or {}).get("triples",[]))
            names,edges,audit,errs=materialize(plan,rows,source,verifier)
            errors.extend(errs)
            if names:
                hops=len(plan["relations"])
                py=python_paths(edges,hops);native=native_paths(a.graph,names,edges,hops)
                if py!=native:raise RuntimeError("native/reference path mismatch")
                decision=decide(names,native);control=decide(names,py)
            else:
                decision=control={"answer":"No answer","status":"invalid_plan","paths":[],"candidates":[]}
            # Matched span-only answer uses same retrieved sentences, same executive and no graph.
            span_ans=get(ep,"Answer the question with shortest answer span using only these passages; if insufficient say No answer.\nQuestion: "+q+"\n"+ctx(ids,corpus),FINAL,64)
            pred[name+"_text_control"]=span_ans["answer"] if span_ans else "No answer"
            pred[name+"_python_graph"]=control["answer"];pred[name+"_moolbase_graph"]=decision["answer"]
            # Preserve single chain as one evidence family; do not inflate independence.
            result={"answer":"No answer","status":"not_attempted","receipt":None}
            if decision["status"]=="resolved":
                paths=[p for p in decision["paths"] if p[0]==decision["answer"] and p[1]=="supported"]
                picked=[source[audit[j]["span"]] for j in paths[0][2]]
                candidate=validate_candidate({"answer":decision["answer"],"citations":[{"id":x["doc"],"quote":x["text"]} for x in picked]},corpus)
                result=native_gate(a.binary,candidate,"support")
            pred[name+"_native_epistemic"]=result["answer"]
            gates[name]={"graph":decision,"epistemic":result,"retrieved":ids,"spans":source,"extractions":audit,"edges":edges,"names":names,"raw_triples":rows}
        aliases=[row["answer"]]+row.get("answer_aliases",[])
        report={"scope":"Q4_K_M CPU; independent NLI; native graph adjacency adapter and matched Python reference; exact/F1 not paper judge","id":qid,"question":q,"aliases":aliases,"models":{k:v for k,v in models.items() if not k.endswith("_path")},"manifest":manifest,"predictions":pred,"scores":{k:score(v,aliases) for k,v in pred.items()},"trace":trace,"plan":plan,"nli_model":NLI_MODEL,"nli_revision":verifier.revision,"gates":gates,"errors":errors,"seconds":time.monotonic()-start}
        Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ("id","predictions","scores","errors","seconds")},indent=2),flush=True)
    finally:
        for proc in [m,e]:
            if proc:
                proc.terminate()
                try:proc.wait(timeout=5)
                except Exception:proc.kill();proc.wait()
if __name__=="__main__":main()
