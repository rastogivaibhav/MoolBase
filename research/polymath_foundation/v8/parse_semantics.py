#!/usr/bin/env python3
"""Fixed deterministic alias parser + actual Qwen semantic normalization.

No oracle, gold, or question-generator import is used in this file.
"""
import argparse,csv,hashlib,json,os,re,time,urllib.request
from pathlib import Path

def digest(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(1048576),b""):h.update(b)
    return h.hexdigest()

def registry(path):
    with open(path,newline="",encoding="utf8") as f:
        r=list(csv.DictReader(f,delimiter="\t"))
    if len(r)!=3 or any(x.get("source_status")!="unverified" for x in r):
        raise ValueError("untrusted manifest")
    return r

def fail(tid,reason,err=None):
    return {"opaque_id":tid,"status":"abstain","reason":reason,
            "normalized":None,"parser_error":err}

def norm(tid,target,unit,inputs,reason=None):
    if not isinstance(target,str) or not target or not isinstance(unit,str) or not unit:
        return fail(tid,"Missing output label/units",err="MISSING_TARGET")
    if not isinstance(inputs,str) or not inputs or "\n" in inputs or "\t" in inputs:
        return fail(tid,"Missing or malformed measurements",err="INVALID_INPUTS")
    parts=inputs.split(";")
    used=set()
    for part in parts:
        m=re.fullmatch(r"([a-z_]+)=(-?[0-9]+(?:\.[0-9]+)?)@([a-z_]+)#(integer|float)",part)
        if not m or m[1] in used:
            return fail(tid,"Bad typed measurement",err="MALFORMED_TYPED_FIELDS")
        used.add(m[1])
        if m[4]=="integer" and "." in m[2]:
            return fail(tid,"Noninteger categorical measurement",err="WRONG_NUMERIC_TYPE")
    return {"opaque_id":tid,"status":"normalize","reason":"-",
            "normalized":{"description":"Natural language semantics require validated execution",
                          "target":target,"target_unit":unit,"inputs":inputs},
            "parser_error":None}

SPECS={
 "networks":[("queue","packets","integer"),("arrivals","packets","integer"),
             ("service","packets","integer"),("buffer","packets","integer")],
 "biology":[("observed","state_units","float"),("setpoint","state_units","float"),
            ("gain","dimensionless","float"),("limit","state_units","float"),
            ("disturbance","state_units","float")],
 "supply_chain":[("daily_demand","items_per_day","integer"),("lead_days","days","integer"),
                 ("safety","items","integer"),("on_hand","items","integer"),
                 ("on_order","items","integer"),("backorders","items","integer"),
                 ("target","items","integer")]
}
PATTERNS={
 "networks":[r"initially (-?\d+) packets wait",r"(-?\d+) packets arrive during",
             r"device serves (-?\d+)",r"buffer capacity is (-?\d+)"],
 "biology":[r"senses (-?\d+(?:\.\d+)?) state units",r"desired (-?\d+(?:\.\d+)?)",
            r"dimensionless sensitivity is (-?\d+(?:\.\d+)?)",r"magnitude ceiling (-?\d+(?:\.\d+)?)",
            r"outside disturbance (-?\d+(?:\.\d+)?)"],
 "supply_chain":[r"usage is (-?\d+) items per day",
                 r"supplier lead time is (-?\d+) days",
                 r"reserve is (-?\d+) items",r"There are (-?\d+) items physically held",
                 r"(-?\d+) inbound",r"(-?\d+) outstanding unmet",
                 r"stock target is (-?\d+) items"]
}
TARGETS={
 "networks":[("how many packets remain waiting","next_queue","packets"),
             ("how many packets get discarded","drops","packets")],
 "biology":[("what corrective actuator output","control","state_units"),
            ("what state reading follows","next_state","state_units")],
 "supply_chain":[("replenishment trigger threshold","reorder_point","items"),
                 ("number of new units to order","order_qty","items")]
}
def rules(question,tid):
    if "TWO independent situations" in question:
        return fail(tid,"Multiple incompatible mechanisms in same request")
    matches=[]
    for domain,patterns in PATTERNS.items():
        values=[re.search(p,question,re.IGNORECASE) for p in patterns]
        if any(x is not None for x in values):
            matches.append((domain,values))
    if len(matches)!=1:
        return fail(tid,"No unique semantic mechanism can be selected")
    domain,values=matches[0]
    outputs=[(target,unit) for alias,target,unit in TARGETS[domain]
             if alias.lower() in question.lower()]
    if len(outputs)!=1:
        return fail(tid,"Output requested is ambiguous")
    out,unit=outputs[0]
    input_parts=[]
    for match,(name,u,kind) in zip(values,SPECS[domain]):
        if match:
            val=match[1]
            if kind=="float" and "." not in val:val+=".0"
            input_parts.append(f"{name}={val}@{u}#{kind}")
    if not input_parts:
        return fail(tid,"No usable quantitative input")
    return norm(tid,out,unit,";".join(input_parts))

def make_system(manifest):
    cards="\n".join(
       "Possible procedure "+d["description"]+
       "; allowed inputs "+d["input_spec"]+
       "; targets "+d["output_spec"]+"." for d in manifest)
    return ("You are a cautious semantic field extractor, not a calculator. "
      "Return ONLY valid JSON with keys status, target, target_unit, inputs, reason. "
      "For one recognizable procedure use status=normalize; inputs MUST be a single "
      "string of fields formatted name=number@unit#integer or #float, separated "
      "with semicolons, using the exact allowed canonical field names/types/units. "
      "Use target and target_unit from the allowlist; never output a mechanism ID, "
      "a calculated answer, or a scientific truth claim. Missing input remains missing. "
      "If there is no supported procedure or two incompatible simultaneous requests, "
      "return status=abstain and a descriptive reason, empty target, target_unit "
      "and inputs. Do not hallucinate measurements. Cards are unverified toy models.\n"
      +cards)

def adapt(tid,raw):
    try:
        o=json.loads(raw)
        if type(o)!=dict or o.get("status") not in ("normalize","abstain"):
            raise ValueError("model returned no recognized status")
        if o["status"]=="abstain":
            reason=o.get("reason")
            if type(reason)!=str or len(reason.strip())<12:
                raise ValueError("model abstention lacks explanation")
            return fail(tid,reason)
        for key in ("target","target_unit","inputs"):
            if type(o.get(key))!=str:raise ValueError("missing "+key)
        return norm(tid,o["target"],o["target_unit"],o["inputs"])
    except (ValueError,TypeError) as ex:
        return fail(tid,"Invalid model response",err=str(ex))

def call_model(endpoint,model,system,question):
    payload={"model":model,"messages":[{"role":"system","content":system},
               {"role":"user","content":question}],"temperature":0,
             "seed":20261021,"max_tokens":240,
             "response_format":{"type":"json_object"},"stream":False}
    req=urllib.request.Request(endpoint.rstrip("/")+"/v1/chat/completions",
        data=json.dumps(payload).encode(),headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req,timeout=250) as response:
        return json.loads(response.read().decode("utf8"))

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--queries",required=True);p.add_argument("--manifest",required=True)
    p.add_argument("--out",required=True);p.add_argument("--arm",choices=["B0","B1","B2"],required=True)
    p.add_argument("--model-file");p.add_argument("--endpoint",default="http://127.0.0.1:8080")
    p.add_argument("--model",default="qwen2.5-1.5b-instruct")
    a=p.parse_args()
    trials=json.loads(Path(a.queries).read_text())
    manifest=registry(a.manifest)
    system=make_system(manifest)
    if a.arm=="B2" and not a.model_file:
        raise SystemExit("real model bytes required")
    report={"experiment":"MOOL-POLYMATH-F8-001","arm":a.arm,
            "actual_model_executed":a.arm=="B2",
            "model_sha256":digest(a.model_file) if a.arm=="B2" else None,
            "questions_sha256":digest(a.queries),
            "github_sha":os.environ.get("GITHUB_SHA","LOCAL"),
            "model_config":{"temperature":0,"seed":20261021,"max_tokens":240},
            "system_prompt":system if a.arm=="B2" else None,"records":[]}
    for i,row in enumerate(trials):
        tid=row["opaque_id"];question=row["question"]
        if a.arm=="B0":
            parsed=fail(tid,"No canonical typed fields available to exact matcher")
            raw=None;usage={};seconds=0
        elif a.arm=="B1":
            parsed=rules(question,tid);raw=None;usage={};seconds=0
        else:
            start=time.monotonic()
            response=call_model(a.endpoint,a.model,system,question)
            seconds=time.monotonic()-start
            raw=response["choices"][0]["message"]["content"]
            usage=response.get("usage",{})
            parsed=adapt(tid,raw)
        report["records"].append({"opaque_id":tid,"question":question,
            "normalized_response":parsed,"raw_model_output":raw,
            "usage":usage,"seconds":seconds})
        Path(a.out).write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
        print(f"f8 {a.arm} {i+1:02d}/24 status={parsed['status']} "
              f"parse_error={parsed['parser_error'] is not None}",flush=True)
if __name__=="__main__":main()
