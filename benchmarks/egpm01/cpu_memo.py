"""CPU-only genuine MeMo checkpoint smoke; simplified protocol, not paper reproduction."""
import argparse,json,re,time,hashlib,unicodedata
from pathlib import Path
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM
from huggingface_hub import model_info
from native_retrieval import prepare,bm25
from hybrid_gate import fused_ranks
MEM="Glow-AI/QWEN2.5-1.5b_instruct-MSQ1000_step1-5_with_N_neg_batchsize256_best_epoch"
EM="Qwen/Qwen2.5-0.5B-Instruct"
def load(model):
    rev=model_info(model).sha
    tok=AutoTokenizer.from_pretrained(model,revision=rev)
    m=AutoModelForCausalLM.from_pretrained(model,revision=rev,torch_dtype=torch.float32).eval()
    return tok,m,rev
def ask(pair,prompt,n=48):
    tok,m,_=pair
    text=tok.apply_chat_template([{"role":"user","content":prompt}],tokenize=False,add_generation_prompt=True)
    x=tok(text,return_tensors="pt",truncation=True,max_length=2048)
    with torch.inference_mode(): y=m.generate(**x,max_new_tokens=n,do_sample=False,pad_token_id=tok.eos_token_id)
    return tok.decode(y[0,x["input_ids"].shape[1]:],skip_special_tokens=True).strip()
def norm(s):
    return " ".join(re.findall(r"\w+",unicodedata.normalize("NFKC",s).casefold()))
def main():
    p=argparse.ArgumentParser();p.add_argument("--out",required=True);p.add_argument("--questions",type=int,default=4);a=p.parse_args()
    torch.set_num_threads(4);torch.manual_seed(0)
    selected,corpus,gold,manifest=prepare(40);rank=bm25(corpus);em=load(EM);mem=load(MEM);out=[]
    for row in selected[:a.questions]:
        t=time.monotonic();q=row["question"];b=rank(q)[:5]
        sub=ask(em,"Write one short factual intermediate question to help answer this question. Output just that question: "+q,40)
        recalled=ask(mem,"Answer the following question from your knowledge. If you do not know, say I don't know. "+sub)
        finalrecall=ask(mem,"Answer the following question from your knowledge. If you do not know, say I don't know. "+q)
        recalls="Intermediate question: "+sub+"\nMemory answer: "+recalled+"\nDirect memory answer: "+finalrecall
        def answer(context):return ask(em,"Answer the question with a short answer only. If insufficient information say No answer.\n"+context+"\nQuestion: "+q,32)
        augmented=fused_ranks([rank(q)[:20],rank(q+" "+sub+" "+recalled+" "+finalrecall)[:20]],5)
        def evidence(ids): return "\n".join(corpus[d][:650] for d in ids)
        predictions={"closed_book":answer(""),"bm25_rag":answer(evidence(b)),"memo_small_simplified":answer(recalls),"memo_augmented_rag_no_moolbase":answer(recalls+"\n"+evidence(augmented))}
        aliases=[row["answer"]]+row.get("answer_aliases",[])
        out.append(dict(id=row["id"],predictions=predictions,exact_match={k:norm(v) in {norm(s) for s in aliases} for k,v in predictions.items()},elapsed_sec=time.monotonic()-t,recall=recalls,bm25=b,augmented=augmented))
        print(row["id"],out[-1]["exact_match"],flush=True)
    report=dict(scope="CPU smoke, simplified MeMo prompt protocol and exact match, not paper judge or full hybrid",memory=MEM,memory_revision=mem[2],executive=EM,executive_revision=em[2],manifest=manifest,n=len(out),scores={k:sum(r["exact_match"][k] for r in out) for k in out[0]["predictions"]},records=out)
    Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!="records"},indent=2),flush=True)
if __name__=="__main__":main()
