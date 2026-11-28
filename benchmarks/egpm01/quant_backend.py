"""Quantised CPU backend. No model weights substituted for the published MeMo checkpoint."""
import json,os,time,urllib.request,subprocess,hashlib
from pathlib import Path
from huggingface_hub import snapshot_download,model_info,list_repo_files,hf_hub_download
MEM="Glow-AI/QWEN2.5-1.5b_instruct-MSQ1000_step1-5_with_N_neg_batchsize256_best_epoch"
MEMREV="48bc49c6d0dd992fa22d96de14b99c11351d6392"
EM="Qwen/Qwen2.5-3B-Instruct-GGUF"
def digest(p):
    with open(p,"rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def prepare_models():
    root=Path("egpm-models");root.mkdir(exist_ok=True)
    rev=model_info(EM).sha
    names=[x for x in list_repo_files(EM,revision=rev) if x.lower().endswith("q4_k_m.gguf")]
    if len(names)!=1:raise RuntimeError("expected one official executive Q4_K_M file: "+str(names))
    em=Path(hf_hub_download(EM,names[0],revision=rev)).resolve()
    mem=Path(snapshot_download(MEM,revision=MEMREV,allow_patterns=["*.safetensors","*.json","*.txt","*.model","*.jinja"])).resolve()
    f16=root/"memo-f16.gguf";q4=root/"memo-q4.gguf"
    subprocess.run(["python","/tmp/llama.cpp/convert_hf_to_gguf.py",str(mem),"--outtype","f16","--outfile",str(f16)],check=True)
    subprocess.run([os.environ["LLAMA_QUANTIZE"],str(f16),str(q4),"Q4_K_M","4"],check=True)
    f16.unlink()
    data={"executive":EM,"executive_revision":rev,"executive_sha256":digest(em),"memory":MEM,"memory_revision":MEMREV,"memory_quantized_sha256":digest(q4),"executive_path":str(em),"memory_path":str(q4.resolve()),"llama_cpp":"b6500","quantization":"Q4_K_M"}
    (root/"manifest.json").write_text(json.dumps(data,indent=2));print(json.dumps(data,indent=2),flush=True)
def request(port,path,body=None,timeout=180):
    req=urllib.request.Request(f"http://127.0.0.1:{port}"+path,data=json.dumps(body).encode() if body is not None else None,headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req,timeout=timeout) as r:return json.load(r)
def serve(path,port,log):
    out=open(log,"w")
    p=subprocess.Popen([os.environ["LLAMA_SERVER"],"-m",path,"--host","127.0.0.1","--port",str(port),"-t","4","-c","8192","-np","1","-ngl","0","--chat-template","chatml"],stdout=out,stderr=out)
    out.close()
    for i in range(180):
        if p.poll() is not None:raise RuntimeError("llama-server exited: "+Path(log).read_text()[-2000:])
        try:
            if request(port,"/health",timeout=2).get("status")=="ok":return p
        except Exception:pass
        time.sleep(1)
    p.terminate();raise RuntimeError("server readiness timeout")
def ask(port,prompt,n=96,schema=None):
    body={"messages":[{"role":"user","content":prompt}],"temperature":0,"max_tokens":n,"seed":42,"stream":False}
    if schema is not None:body["response_format"]={"type":"json_object","schema":schema}
    r=request(port,"/v1/chat/completions",body)
    choice=r["choices"][0];content=choice["message"]["content"].strip()
    if schema is not None:
        if choice.get("finish_reason")=="length":raise ValueError("constrained output hit token cap")
        obj=json.loads(content)
        if not isinstance(obj,dict):raise ValueError("not JSON object")
    return content
if __name__=="__main__":prepare_models()
