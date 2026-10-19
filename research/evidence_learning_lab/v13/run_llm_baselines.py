#!/usr/bin/env python3
"""Real 7B model: direct event JSON vs exact timestamped retrieval.

This module loads ONLY public JSONL event records (never controller oracle or
SQLite DB) and strictly excludes timepoint T2 from model requests.
"""
import argparse,hashlib,json,os,time,urllib.request
from pathlib import Path

SYSTEM=("You investigate operational stock incidents. All sources are a controlled "
"synthetic ERP/WMS lab; their statements are unverified, not established truths. "
"Choose exactly one of: H-A (ERP receipt posting delay), "
"H-B (stale WMS snapshot), H-C (incorrect replenishment threshold), "
"or none (insufficient/ambiguous evidence). Do not fabricate measurements. "
"Do not assume a later event exists. Output ONLY JSON with keys "
"choice and reason. Keep reason under 30 words. "
"Choose none when diagnostic data cannot distinguish explanations.")

def digest(path):
 h=hashlib.sha256()
 with open(path,"rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def load_public(path):
 logs=[]
 for name in sorted(Path(path).glob("LAB-*/events.jsonl")):
  for line in name.read_text().splitlines():
   r=json.loads(line)
   if r.get("source") not in ("erp_adapter","wms_adapter","order_service") or r.get("provenance_class")!="sandbox_self_reported_unverified":
    raise ValueError("unreviewed or authenticated-looking producer metadata")
   if r.get("stage") not in ("T0","T1","T2"):
    raise ValueError("invalid stage")
   logs.append(r)
 if len(logs)!=30:raise ValueError("expected 30 executed-source events")
 return logs
def selected(logs,case_id,cutoff):
 keep={"T0"} if cutoff=="T0" else {"T0","T1"}
 records=[r for r in logs if r["case_id"]==case_id and r["stage"] in keep]
 records.sort(key=lambda e:(e["event_time"],e["event_id"]))
 if len(records)!=(1 if cutoff=="T0" else 3):
  raise ValueError("missing or future contaminated event stream")
 return records
def direct(records):
 # all event data, no database retrieval stage; same underlying source records
 return json.dumps(records,sort_keys=True,separators=(",",":"),ensure_ascii=False)
def retrieval(records):
 # same exact records retrieved with case/time exact-index filter, rearranged
 # as citation labelled event snippets. NOT a learned or semantic RAG.
 lines=[]
 for r in records:
  lines.append(f"[{r['event_id']} | case={r['case_id']} | stage={r['stage']} | "
               f"time={r['event_time']} | system={r['source']} | "
               f"event_type={r['event_type']} | provenance={r['provenance_class']}] "
               +json.dumps(r["payload"],sort_keys=True,separators=(",",":")))
 return "\n".join(lines)
def complete(endpoint,model,question):
 payload={"model":model,
 "messages":[{"role":"system","content":SYSTEM},
             {"role":"user","content":question}],
 "temperature":0,"seed":20261019,"max_tokens":160,
 "response_format":{"type":"json_object"},"stream":False}
 req=urllib.request.Request(endpoint.rstrip("/")+"/v1/chat/completions",
     data=json.dumps(payload).encode(),headers={"Content-Type":"application/json"})
 with urllib.request.urlopen(req,timeout=900) as response:
  return json.loads(response.read().decode())
def parse(raw):
 try:
  result=json.loads(raw)
  if type(result)!=dict or set(result)!={"choice","reason"}:
   raise ValueError("incorrect response schema")
  if result["choice"] not in ("H-A","H-B","H-C","none"):
   raise ValueError("unknown diagnosis")
  if type(result["reason"])!=str or not result["reason"].strip():
   raise ValueError("missing justification")
  return result["choice"],result["reason"],None
 except (TypeError,ValueError) as error:
  return "none","MALFORMED_MODEL_OUTPUT",str(error)
def main():
 p=argparse.ArgumentParser()
 for key in ("public","model-file","out"):
  p.add_argument("--"+key,required=True)
 p.add_argument("--endpoint",default="http://127.0.0.1:8080")
 p.add_argument("--model",default="qwen2.5-7b-instruct")
 a=p.parse_args()
 logs=load_public(a.public)
 cases=sorted({r["case_id"] for r in logs})
 if len(cases)!=6:raise ValueError("wrong experimental population")
 out={"experiment":"MOOL-EVIDENCE-LEARNING-V13B-001",
   "real_model_executed":True,"source_authenticated":False,
   "external_ground_truth_accessed":False,
   "model":a.model,"model_sha256":digest(a.model_file),
   "git_sha":os.getenv("GITHUB_SHA","LOCAL"),
   "system_prompt":SYSTEM,
   "settings":{"temperature":0,"seed":20261019,"max_tokens":160,
               "ctx_size":4096},
   "records":[]}
 for case_id in cases:
  for cutoff in ("T0","T1"):
   events=selected(logs,case_id,cutoff)
   for arm in ("L0","L1"):
    context=direct(events) if arm=="L0" else retrieval(events)
    prompt=("Investigate "+case_id+" using ONLY the following event observations "
            "at "+cutoff+". Which explanation currently best fits, if any?\n"
            +context)
    start=time.monotonic()
    result=complete(a.endpoint,a.model,prompt)
    seconds=time.monotonic()-start
    raw=result["choices"][0]["message"]["content"]
    if not isinstance(raw,str):raise RuntimeError("actual model returned nontext")
    choice,reason,error=parse(raw)
    out["records"].append({"case_id":case_id,"cutoff":cutoff,
      "arm":arm,"events_used":[x["event_id"] for x in events],
      "question":prompt,"raw_model_response":raw,
      "parsed_choice":choice,"reason":reason,"parse_error":error,
      "seconds":seconds,"usage":result.get("usage",{}),
      "finish_reason":result["choices"][0].get("finish_reason")})
    Path(a.out).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")
    print("v13b real-7b",len(out["records"]),"/24",case_id,cutoff,arm,
          "choice="+choice,"bad_json="+str(error is not None),flush=True)
 print("v13b_exact_24_real_inference=PASS zero_future_oracle_in_prompts=YES")
if __name__=="__main__":main()
