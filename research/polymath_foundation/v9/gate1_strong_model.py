#!/usr/bin/env python3
"""F9 Gate 1: real pinned 7B semantic extraction; v8 score remains unchanged."""
import argparse,hashlib,json,os,sys,time,urllib.request
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v8"))
from parse_semantics import registry,adapt,digest

def system_prompt(rows):
    # Restricted field dictionary comes from published pedagogical cards;
    # no sample question, gold answer, test category, or mathematical result.
    domains=[]
    for r in rows:
        domains.append("Mechanism "+r["description"]+
          "; inputs "+r["input_spec"]+
          "; possible outputs "+r["output_spec"])
    return (
      "TASK: translate ONE user problem into a structured record. NEVER calculate a "
      "numeric answer. NEVER copy field declarations as if they were measurements. "
      "Extract actual NUMERIC VALUES from the question and label them according "
      "to the appropriate reviewed toy mechanism. Do not assume missing values. "
      "If the request is not from exactly one supported mechanism, abstain. "
      "If one supported mechanism is present but the measured value violates an "
      "assumption, extract the values anyway: the downstream validator must "
      "determine whether to abstain. "
      "Reply as ONE JSON object, no Markdown, keys exactly: "
      "status, target, target_unit, inputs, reason. "
      "status must be normalize or abstain. Normalize format is exact: "
      "'inputs' is a single string 'name=number@unit#integer;name=number@unit#float'. "
      "Use EXACT allowed canonical input names and declared units/types, not the "
      "field descriptor string 'name:float:unit'. Select the target NAME only, "
      "not a type or unit descriptor. Preserve sign and decimals. "
      "For unsupported/multi-mechanism questions output "
      "{\"status\":\"abstain\",\"target\":\"\",\"target_unit\":\"\","
      "\"inputs\":\"\",\"reason\":\"No single supported procedure applies\"}. "
      "One format example, NOT a test case: 'Initially 2 packets wait; 3 arrive; "
      "1 is served; capacity 5. How many packets remain waiting?' => "
      "{\"status\":\"normalize\",\"target\":\"next_queue\","
      "\"target_unit\":\"packets\","
      "\"inputs\":\"queue=2@packets#integer;arrivals=3@packets#integer;"
      "service=1@packets#integer;buffer=5@packets#integer\","
      "\"reason\":\"One supported procedure\"}. "
      "Example gives NO calculated result. Source material is unverified toy "
      "knowledge; do not call it experimentally verified. "
      "\nAvailable reviewed mechanisms:\n"+"\n".join(domains)
    )

def query(server,model,sys_prompt,q):
    payload={"model":model,"messages":[{"role":"system","content":sys_prompt},
                     {"role":"user","content":q}],
             "temperature":0,"seed":20261022,"max_tokens":384,
             "response_format":{"type":"json_object"},"stream":False}
    req=urllib.request.Request(server.rstrip("/")+"/v1/chat/completions",
          data=json.dumps(payload).encode(),headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req,timeout=900) as res:
        return json.loads(res.read().decode())

def main():
    p=argparse.ArgumentParser()
    for name in ("queries","manifest","model-file","output"):
        p.add_argument("--"+name,required=True)
    p.add_argument("--server",default="http://127.0.0.1:8080")
    p.add_argument("--model",default="qwen2.5-7b-instruct")
    a=p.parse_args()
    trials=json.loads(Path(a.queries).read_text())
    assert len(trials)==24
    manifest=registry(a.manifest)
    system=system_prompt(manifest)
    out={"experiment":"MOOL-POLYMATH-F9-G1-001",
         "score_source":"original_v8_scorer_unchanged",
         "actual_model_executed":True,
         "source_corpus":"v8_project_authored_not_independent_blind",
         "model_name":a.model,"model_sha256":digest(a.model_file),
         "github_sha":os.environ.get("GITHUB_SHA","LOCAL"),
         "questions_sha256":digest(a.queries),
         "model_config":{"temperature":0,"seed":20261022,"max_tokens":384,
                         "context":4096},
         "system_prompt":system,"records":[]}
    for i,task in enumerate(trials):
        start=time.monotonic()
        response=query(a.server,a.model,system,task["question"])
        elapsed=time.monotonic()-start
        choice=response["choices"][0]
        raw=choice["message"]["content"]
        if not isinstance(raw,str):
            raise RuntimeError("inference returned nontext output")
        parsed=adapt(task["opaque_id"],raw)
        out["records"].append({"opaque_id":task["opaque_id"],"question":task["question"],
             "normalized_response":parsed,"raw_model_output":raw,
             "usage":response.get("usage",{}),"seconds":elapsed,
             "finish_reason":choice.get("finish_reason")})
        # Persist raw responses even if later requests fail. Partial != scored.
        Path(a.output).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")
        print(f"F9 G1 real 7B: {i+1:02d}/24 status={parsed['status']} "
              f"invalid={parsed['parser_error'] is not None} elapsed={elapsed:.2f}",
              flush=True)
    print("f9_gate1_actual_7b_inference=PASS n=24",flush=True)
if __name__=="__main__":
    main()
