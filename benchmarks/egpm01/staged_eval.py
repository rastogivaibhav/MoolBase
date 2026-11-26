"""EGPM06 staged evidence extraction; fixed-budget iterative retrieval and oracle diagnostics."""
import argparse,json,time
from pathlib import Path
from native_retrieval import prepare,bm25
from quant_eval import obj,STR,GROUND,PIN,FINAL,structured,ctx,score
from quant_backend import serve
from independent_nli import Verifier,MODEL as NLI_MODEL
from hybrid_gate import fused_ranks,native_gate,validate_candidate
from staged_graph import State,valid_plan,select_windows
PLAN=obj({"anchor":STR,"relations":{"type":"array","items":{"type":"string","maxLength":100},"minItems":2,"maxItems":4}})
MAX_DOCS=16
MAX_FRONTIER=3
MAX_WINDOWS=10
MAX_NEW_DOCS=3
def extraction_schema(ids):
    # Restricted enum, not arbitrary string. Stage is controlled by caller.
    return obj({"facts":{"type":"array","minItems":0,"maxItems":4,"items":obj({"subject":STR,"object":STR,"span":{"type":"string","enum":list(ids)}})}})
def graph_arm(plan,q,initial,corpus,rank,get,verify,binary,active):
    if not valid_plan(plan,q):return {"answer":"No answer","status":"invalid_plan","paths":[],"candidates":[],"retrieved":initial,"audit":[],"errors":["invalid_plan"],"stages":[]}
    state=State(plan,verify,binary);docs=list(dict.fromkeys(initial));stages=[]
    for stage,rel in enumerate(plan["relations"]):
        frontier=state.frontier(stage)
        if not frontier:break
        before=list(docs);limited=frontier[:MAX_FRONTIER];truncated=len(frontier)>MAX_FRONTIER
        for subject in limited:
            def extract():
                source=select_windows(subject,rel,docs,corpus,MAX_WINDOWS)
                if not source:return
                text="\n".join(k+": "+v["text"] for k,v in source.items())
                raw=get("Extract only explicitly stated facts completing this relationship. Subject must be '"+subject+"' or an explicitly identified alias. Return subject and object exactly as they appear in the selected source window. Include explicit negation of the same relation; do not infer. If no evidence return empty facts. Relation: "+rel+"\nEvidence windows:\n"+text,extraction_schema(source),256)
                state.add(stage,subject,(raw or {}).get("facts",[]),source)
            extract()
            # Only retrieve after missing supported continuation for this exact subject.
            eligible=[e for e in state.edges if e[0]==state.index.get(" ".join(__import__('re').findall(r"\w+",subject.casefold()))) and e[2]==stage and e[3]=="S"]
            if active and not eligible and len(docs)<MAX_DOCS:
                new=[d for d in rank(subject+" "+rel+" "+q) if d not in docs][:min(MAX_NEW_DOCS,MAX_DOCS-len(docs))]
                docs.extend(new)
                if new:extract()
        stages.append({"stage":stage,"relation":rel,"frontier":frontier,"frontier_truncated":truncated,"new_docs":[d for d in docs if d not in before],"accepted_edges":len([e for e in state.edges if e[2]==stage])})
        if truncated:
            return {"answer":"No answer","status":"frontier_budget_exceeded","paths":[],"candidates":[],"retrieved":docs,"audit":state.audit,"errors":state.errors,"stages":stages,"spans":state.source,"edges":state.edges,"names":state.names}
    d=state.result()
    return {**d,"retrieved":docs,"audit":state.audit,"errors":state.errors,"stages":stages,"spans":state.source,"edges":state.edges,"names":state.names}
def main():
    p=argparse.ArgumentParser();p.add_argument("--binary",required=True);p.add_argument("--graph",required=True);p.add_argument("--offset",type=int,required=True);p.add_argument("--out",required=True);a=p.parse_args()
    rows,corpus,gold,manifest=prepare(120);rank=bm25(corpus);row=rows[a.offset];q=row["question"];qid=row["id"]
    models=json.loads(Path("egpm-models/manifest.json").read_text());verifier=Verifier();ep,mp=18081,18082
    e=serve(models["executive_path"],ep,"egpm-models/executive.log");m=None
    try:
        m=serve(models["memory_path"],mp,"egpm-models/memory.log");start=time.monotonic();errors=[];trace=[]
        def call(port,prompt,schema,n=192):
            result,err=structured(port,prompt,schema,n);errors.extend(err);return result
        def get(prompt,schema,n=192):return call(ep,prompt,schema,n)
        def short(ids):
            r=get("Give the shortest answer span. If insufficient evidence say No answer.\nQuestion: "+q+"\nPassages:\n"+ctx(ids,corpus),FINAL,64)
            return r["answer"] if r else "No answer"
        b=rank(q)[:10];pred={"closed_book":short([]),"A_rag":short(b)}
        g=get("Split the question into one or two atomic factual questions.\n"+q,GROUND,192)
        def recall(s):
            r=call(mp,"Answer this question from memory. If uncertain say I don't know.\n"+s,FINAL,96)
            trace.append({"question":s,"answer":r["answer"] if r else "I don't know"})
        for s in (g or {}).get("questions",[])[:2]:recall(s)
        pin=get("Identify an intermediate entity and ask one follow-up question.\nOriginal: "+q+"\nReplies: "+json.dumps(trace),PIN,192)
        if pin and pin["question"]:recall(pin["question"])
        f=get("Synthesize the shortest answer from memory. If uncertain say No answer.\nQuestion: "+q+"\nMemory: "+json.dumps(trace),FINAL,64)
        pred["B_memo"]=f["answer"] if f else "No answer"
        aug=fused_ranks([rank(q)[:20],rank(q+" "+json.dumps(trace)+" "+pred["B_memo"])[:20]],10)
        plan=get("Make a directed chain of 2 to 4 atomic relationships from an exact named anchor in the question to the final answer. Do not use composite relations such as grandmother: use two parent relationships. Each stage must start at the preceding stage's object, and the last stage must answer the question. Example: Who founded the company that distributed Film X? anchor Film X, relations ['was distributed by','was founded by']. Do not assume any missing entity.\nQuestion: "+q,PLAN,192)
        arms={}
        # Ordinary arms never receive gold evidence, aliases or decomposition.
        for name,ids,active in [("C_static",b,False),("D_static",aug,False),("C_active",b,True),("D_active",aug,True)]:
            arm=graph_arm(plan,q,ids,corpus,rank,get,verifier,a.graph,active)
            arms[name]=arm;pred[name+"_graph"]=arm["answer"];pred[name+"_matched_text"]=short(arm["retrieved"])
            # Separate policy arm. Each full path is one correlated evidence family.
            result={"answer":"No answer","status":"not_attempted","receipt":None}
            if arm["status"]=="resolved":
                path=next(p for p in arm["paths"] if p[0]==arm["answer"] and p[1]=="supported")
                picked=[arm["spans"][arm["audit"][j]["span"]] for j in path[2]]
                candidate=validate_candidate({"answer":arm["answer"],"citations":[{"id":x["doc"],"quote":x["text"]} for x in picked]},corpus)
                result=native_gate(a.binary,candidate,"support")
            arm["epistemic"]=result;pred[name+"_epistemic"]=result["answer"]
        # Oracle only after all ordinary predictions; same plan/extractor/NLI with true documents.
        oracle_ids=sorted(gold[qid]);pred["oracle_text"]=short(oracle_ids)
        oracle=graph_arm(plan,q,oracle_ids,corpus,rank,get,verifier,a.graph,False);arms["oracle_graph"]=oracle;pred["oracle_graph"]=oracle["answer"]
        aliases=[row["answer"]]+row.get("answer_aliases",[])
        report={"scope":"8-question development pilot; adapted MeMo; independent NLI; static/active matched-document controls; oracle graph diagnostic; exact/F1 not paper judge","id":qid,"question":q,"aliases":aliases,"plan":plan,"models":{k:v for k,v in models.items() if not k.endswith("_path")},"nli_model":NLI_MODEL,"nli_revision":verifier.revision,"manifest":manifest,"trace":trace,"predictions":pred,"scores":{k:score(v,aliases) for k,v in pred.items()},"arms":arms,"errors":errors,"seconds":time.monotonic()-start}
        Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2))
        print(json.dumps({**{k:report[k] for k in ("id","predictions","scores","errors","seconds")},"diagnostics":{k:{"status":v["status"],"accepted_edges":len(v.get("edges",[])),"audited":len(v["audit"]),"errors":v["errors"],"stages":v["stages"]} for k,v in arms.items()}},indent=2),flush=True)
    finally:
        for proc in [m,e]:
            if proc:
                proc.terminate()
                try:proc.wait(timeout=5)
                except Exception:proc.kill();proc.wait()
if __name__=="__main__":main()
