"""CPU-only matched QA pilot. Original MeMo structured inference functions, bounded CPU turns.
No gold in any non-oracle prompt. Same executive and corpus for all arms.
"""
import argparse,ast,asyncio,json,re,sys,time,types,runpy
from pathlib import Path
import torch
from transformers import AutoModelForCausalLM,AutoTokenizer
from huggingface_hub import model_info
from native_retrieval import prepare,bm25
from hybrid_gate import fused_ranks,native_gate,validate_candidate
from cpu_memo import norm
MEM="Glow-AI/QWEN2.5-1.5b_instruct-MSQ1000_step1-5_with_N_neg_batchsize256_best_epoch"
EM="Qwen/Qwen2.5-3B-Instruct"
def load(name,revision=None):
    revision=revision or model_info(name).sha
    t=AutoTokenizer.from_pretrained(name,revision=revision)
    m=AutoModelForCausalLM.from_pretrained(name,revision=revision,torch_dtype=torch.bfloat16).eval()
    return t,m,revision
def ask(pair,prompt,n=96):
    t,m,_=pair
    x=t(t.apply_chat_template([{"role":"user","content":prompt}],tokenize=False,add_generation_prompt=True),return_tensors="pt")
    if x["input_ids"].shape[1]>8192: raise ValueError("prompt exceeds fixed 8192-token budget")
    with torch.inference_mode(): y=m.generate(**x,max_new_tokens=n,do_sample=False,pad_token_id=t.eos_token_id,temperature=None,top_p=None,top_k=None)
    return t.decode(y[0,x["input_ids"].shape[1]:],skip_special_tokens=True).strip()
class Local:
    def __init__(self,pair,cap):
        self.pair=pair;self.cap=cap;self.chat=types.SimpleNamespace(completions=self)
    async def create(self,**kw):
        content=ask(self.pair,kw["messages"][0]["content"],min(self.cap,kw.get("max_tokens",self.cap)))
        return types.SimpleNamespace(choices=[types.SimpleNamespace(message=types.SimpleNamespace(content=content))])
def original_functions(upstream):
    # Load original prompt functions and evaluator function bodies from the pinned checkout.
    # Exclude CLI imports/entrypoint that depend on remote scoring and unused data modules.
    up=Path(upstream).resolve()
    ns={"asyncio":asyncio,"json":json,"re":re}
    ns.update(runpy.run_path(str(up/"evaluation_pipeline/general_eval_prompt_utils.py")))
    sys.path.insert(0,str(up/"data_synthesis_pipeline"))
    from model_utils import prepare_prompt_for_checking_mem_knowledge
    ns["prepare_prompt_for_checking_mem_knowledge"]=prepare_prompt_for_checking_mem_knowledge
    path=up/"evaluation_pipeline/structured_multi_turn/eval_msq_trng_structured_multi_step.py"
    tree=ast.parse(path.read_text())
    selected=[n for n in tree.body if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef)) and n.name not in ("main","load_questions")]
    exec(compile(ast.Module(body=selected,type_ignores=[]),str(path),"exec"),ns)
    return ns["process_single_question"]
def jparse(s):
    s=s.strip()
    if s.startswith("```"):s=re.sub(r"^```(?:json)?\s*|\s*```$","",s)
    return json.loads(s)
def context(ids,corpus):
    return "\n\n".join("["+d+"] "+corpus[d] for d in ids)
def candidate(em,q,docs,recalls=""):
    prompt="Answer the question using the provided passages. Recall is a hint, not evidence. Return only JSON with answer (short span) and citations (list of objects with id and exact verbatim quote). If insufficient evidence return an empty answer and empty citations.\nQuestion: "+q+"\nRecall: "+recalls+"\nPassages:\n"+context(docs,list_to_dict(docs))
    raw=ask(em,prompt,384)
    try:return validate_candidate(jparse(raw),docs),raw
    except (ValueError,KeyError,TypeError):return None,raw
def list_to_dict(d):return d
def verdict(em,q,c,docs):
    quotes="\n".join("["+x["id"]+"] "+x["quote"] for x in c["citations"])
    raw=ask(em,"Check whether the quoted evidence establishes the complete answer to the question, including all necessary links. Use only the quotes; ignore prior knowledge. Reply exactly support, refute, or unresolved.\nQuestion: "+q+"\nAnswer: "+c["answer"]+"\nQuotes:\n"+quotes,12)
    v=raw.strip().lower().strip(".")
    return (v if v in ("support","refute","unresolved") else "unresolved"),raw
def score(pred,aliases):
    p=norm(pred);a=[norm(x) for x in aliases if x]
    exact=bool(p) and p in a
    # token F1 (not the paper's DeepEval semantic judge)
    from collections import Counter
    def f1(x):
        t=p.split();u=x.split();over=sum((Counter(t)&Counter(u)).values())
        return 2*over/(len(t)+len(u)) if t and u else 0.0
    return {"exact":exact,"f1":max([f1(x) for x in a],default=0)}
def main():
    p=argparse.ArgumentParser();p.add_argument("--upstream",required=True);p.add_argument("--binary",required=True);p.add_argument("--out",required=True);p.add_argument("--questions",type=int,default=4);p.add_argument("--offset",type=int,default=0);a=p.parse_args()
    torch.set_num_threads(4);torch.manual_seed(0)
    rows,corpus,gold,manifest=prepare(120);rank=bm25(corpus)
    em=load(EM);mem=load(MEM,"48bc49c6d0dd992fa22d96de14b99c11351d6392")
    process=original_functions(a.upstream);out=[]
    for row in rows[a.offset:a.offset+a.questions]:
        q=row["question"];qid=row["id"];aliases=[row["answer"]]+row.get("answer_aliases",[])
        t=time.monotonic()
        b=rank(q)[:5];oracle=sorted(gold[qid])
        def answer(ids):return ask(em,"Answer the question with the shortest answer span. If insufficient evidence say No answer.\nQuestion: "+q+"\nPassages:\n"+context(ids,corpus),48)
        pred={"closed_book":answer([]),"A_rag":answer(b),"oracle":answer(oracle)}
        # Original inference functions; the groundtruth field is merely carried to output by upstream.
        # Supply empty labels instead to make leakage impossible.
        trace=asyncio.run(process(Local(em,512),Local(mem,192),{"question":q,"groundtruth":[]},qid,512,-1,False,lm_model_id=EM,sm_model_id=MEM,max_entity_turns=2,max_answer_turns=2,dead_end_threshold=2,sm_max_concurrent=1))
        pred["B_memo"]=trace.get("model_final_response","No answer")
        if not isinstance(pred["B_memo"],str):pred["B_memo"]="No answer"
        recalls="\n".join(x["question"]+" "+x["answer"] for x in trace.get("all_sm_responses_filtered",[]) if isinstance(x.get("answer"),str))[:3000]
        # No gold or answer aliases in recall-assisted search.
        aug=fused_ranks([rank(q)[:20],rank(q+" "+recalls+" "+pred["B_memo"])[:20]],5)
        gates={}
        for name,ids,hint in [("C",b,""),("D",aug,recalls)]:
            docs={d:corpus[d] for d in ids}
            c,raw=candidate(em,q,docs,hint)
            v,vraw=verdict(em,q,c,docs) if c else ("unresolved","invalid or missing candidate")
            pred[name+"_verifier_only"]=c["answer"] if c and v=="support" else "No answer"
            result=native_gate(a.binary,c,v) if c else {"answer":"No answer","status":"invalid_candidate","receipt":None}
            pred[name+"_moolbase"]=result["answer"]
            gates[name]={"candidate":c,"raw_candidate":raw,"verdict":v,"verifier_raw":vraw,"native":result,"retrieved":ids}
        rec=dict(id=qid,question=q,aliases=aliases,predictions=pred,scores={k:score(v,aliases) for k,v in pred.items()},trace=trace,gates=gates,seconds=time.monotonic()-t)
        out.append(rec);print(json.dumps({"id":qid,"predictions":pred,"scores":rec["scores"],"trace_error":trace.get("error"),"seconds":rec["seconds"]}),flush=True)
        report=dict(scope="bounded original structured inference functions; greedy BF16 CPU; 2+2 turns; same-model separate verifier; exact/F1 not paper judge",memory=MEM,memory_revision=mem[2],executive=EM,executive_revision=em[2],manifest=manifest,n=len(out),scores={k:{"exact":sum(r["scores"][k]["exact"] for r in out)/len(out),"f1":sum(r["scores"][k]["f1"] for r in out)/len(out)} for k in pred},records=out)
        Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!="records"},indent=2),flush=True)
if __name__=="__main__":main()
