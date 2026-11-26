"""Evidence spans and native directed graph adapter; Python reference is a matched ablation."""
import re,json,hashlib,subprocess,tempfile
from pathlib import Path
from collections import defaultdict
def normalize(s):return " ".join(re.findall(r"\w+",s.casefold()))
def spans(ids,corpus):
    result={}
    for j,d in enumerate(ids):
        text=corpus[d]
        # Keep exact source offsets; split without regenerating text. Preserve title context in source.
        for i,m in enumerate(re.finditer(r"[^.!?\n]+(?:[.!?]+|$)",text)):
            v=m.group().strip()
            if not v:continue
            start=m.start()+len(m.group())-len(m.group().lstrip())
            result[f"d{j}s{i}"]={"doc":d,"text":v,"start":start,"end":start+len(v),"sha256":hashlib.sha256(v.encode()).hexdigest()}
    return result
def materialize(plan,rows,source,verify):
    if not plan or not normalize(plan.get("anchor","")) or not 2<=len(plan.get("relations",[]))<=4:return [],[],[],["invalid_plan"]
    names=[plan["anchor"]];indices={normalize(names[0]):0};edges=[];audit=[];errors=[]
    def entity(s):
        k=normalize(s)
        if k not in indices:indices[k]=len(names);names.append(s.strip())
        return indices[k]
    for t in rows:
        if not isinstance(t,dict):errors.append("invalid_triple");continue
        sid=t.get("span");stage=t.get("stage");sub=t.get("subject","");ob=t.get("object","")
        if sid not in source or not isinstance(stage,int) or not 0<=stage<len(plan["relations"]) or not normalize(sub) or not normalize(ob) or normalize(sub)==normalize(ob):errors.append("invalid_triple");continue
        evidence=source[sid]["text"];rel=plan["relations"][stage]
        # Avoid hallucinated endpoints and claims based only on memory. Exact normalized mention required.
        if any(" "+normalize(x)+" " not in " "+normalize(evidence)+" " for x in (sub,ob)):errors.append("entity_not_in_span");continue
        claim=sub+" "+rel+" "+ob+"."
        verdict,probs=verify(evidence,claim)
        item={"subject":sub,"object":ob,"stage":stage,"relation":rel,"span":sid,"claim":claim,"verdict":verdict,"probabilities":probs}
        idx=len(audit);audit.append(item)
        if verdict in ("support","refute"):
            edges.append((entity(sub),entity(ob),stage,"S" if verdict=="support" else "R",idx))
    return names,edges,audit,errors
def python_paths(edges,hops):
    adj=defaultdict(list)
    for e in edges:adj[e[0]].append(e)
    out=[]
    def dfs(u,stage,path,visited,refuted,contested):
        if stage==hops:
            out.append((u,"contested" if contested else "refuted_chain" if refuted else "supported",tuple(path)));return
        options=[e for e in adj[u] if e[2]==stage]
        for e in options:
            _,v,_,sign,i=e
            if v in visited:continue
            conflict=len({x[3] for x in options if x[1]==v})>1
            dfs(v,stage+1,path+[i],visited|{v},refuted or sign=="R",contested or conflict)
    dfs(0,0,[],{0},False,False);return sorted(out)
def native_paths(binary,names,edges,hops):
    with tempfile.TemporaryDirectory() as tmp:
        f=Path(tmp)/"input";f.write_text(f"{len(names)} {len(edges)} 0 {hops}\n"+"".join(" ".join(map(str,e))+"\n" for e in edges))
        r=subprocess.run([str(Path(binary).resolve()),str(Path(tmp)/"db"),str(f)],check=True,capture_output=True,text=True,timeout=60)
    out=[]
    for line in r.stdout.splitlines():
        u,s,*path=line.split();out.append((int(u),s,tuple(map(int,path))))
    return sorted(out)
def decide(names,paths):
    supported={p[0] for p in paths if p[1]=="supported"}
    disputed={p[0] for p in paths if p[1]=="contested"}
    eligible=supported-disputed
    # A refuted route is not proof that the endpoint itself is false.
    status="resolved" if len(eligible)==1 and not disputed else "ambiguous" if len(eligible)>1 else "contradictory" if disputed else "refuted_chain" if paths else "missing_evidence"
    answer=names[next(iter(eligible))] if status=="resolved" else "No answer"
    return {"answer":answer,"status":status,"candidates":[names[i] for i in sorted(supported|disputed)],"paths":[[names[u],s,list(p)] for u,s,p in paths]}
