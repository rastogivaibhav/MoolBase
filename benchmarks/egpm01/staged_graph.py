"""EGPM06: immutable source windows, strict ID validation, staged graph and diagnostics."""
import hashlib,re,json
from graph_adapter import normalize,native_paths,python_paths,decide
def canonical_id(s,allowed):
    if not isinstance(s,str):return None
    s=s.strip()
    if s.startswith("[") and s.endswith("]"):s=s[1:-1].strip()
    return s if s in allowed else None
def windows(ids,corpus):
    # Entire short paragraphs or bounded overlapping source windows; no synthetic text.
    out={}
    for d in dict.fromkeys(ids):
        text=corpus[d]
        starts=[0]+[m.end() for m in re.finditer(r"[.!?]\s+|\n+",text)]
        starts=sorted(set(s for s in starts if s<len(text)))
        for i,start in enumerate(starts):
            end=starts[min(i+2,len(starts)-1)] if i+2<len(starts) else len(text)
            end=min(end,start+1200)
            value=text[start:end].strip()
            if not value:continue
            a=start+len(text[start:end])-len(text[start:end].lstrip());z=a+len(value)
            sid="e"+hashlib.sha256((d+"\0"+str(a)+"\0"+str(z)).encode()).hexdigest()[:12]
            record={"doc":d,"start":a,"end":z,"text":value,"sha256":hashlib.sha256(value.encode()).hexdigest()}
            if sid in out and out[sid]!=record:raise RuntimeError("span ID collision")
            out[sid]=record
    return out
def mention(entity,text):
    n=normalize(entity)
    return bool(n) and " "+n+" " in " "+normalize(text)+" "
def safe_alias(a,b,premise,verify):
    # Alias joining requires both mentions and an independently supported explicit identity claim.
    if normalize(a)==normalize(b):return True,{}
    if not mention(a,premise) or not mention(b,premise):return False,{}
    v,p=verify(premise,a+" is the same entity as "+b+".")
    return v=="support",p
def valid_plan(plan,q):
    return isinstance(plan,dict) and isinstance(plan.get("anchor"),str) and mention(plan["anchor"],q) and isinstance(plan.get("relations"),list) and 2<=len(plan["relations"])<=4 and all(isinstance(r,str) and 1<=len(r.strip())<=100 for r in plan["relations"])
class State:
    def __init__(self,plan,verify,binary):
        self.plan=plan;self.verify=verify;self.binary=binary;self.names=[plan["anchor"]];self.index={normalize(self.names[0]):0};self.edges=[];self.audit=[];self.errors=[];self.seen=set();self.source={}
    def node(self,s):
        n=normalize(s)
        if n not in self.index:self.index[n]=len(self.names);self.names.append(s.strip())
        return self.index[n]
    def add(self,stage,subject,rows,source):
        self.source.update(source)
        for item in rows:
            if not isinstance(item,dict):self.errors.append("invalid_item");continue
            sid=canonical_id(item.get("span"),source)
            target=item.get("object");surface=item.get("subject")
            if sid is None:self.errors.append("invalid_span_id");continue
            if not isinstance(target,str) or not isinstance(surface,str) or not normalize(target) or len(target.encode())>200 or normalize(subject)==normalize(target):self.errors.append("invalid_endpoint");continue
            evidence=source[sid]["text"]
            if not mention(surface,evidence) or not mention(target,evidence):self.errors.append("endpoint_not_in_span");continue
            if normalize(surface)!=normalize(subject):
                ok,p=safe_alias(subject,surface,evidence,self.verify)
                if not ok:self.errors.append("unverified_alias");continue
            else:p={}
            key=(stage,normalize(subject),normalize(target),sid)
            if key in self.seen:continue
            self.seen.add(key)
            claim=surface+" "+self.plan["relations"][stage]+" "+target+"."
            verdict,probs=self.verify(evidence,claim)
            idx=len(self.audit);self.audit.append({"stage":stage,"subject":subject,"surface":surface,"object":target,"span":sid,"claim":claim,"verdict":verdict,"probabilities":probs,"alias_probabilities":p})
            if verdict in ("support","refute"):
                self.edges.append((self.node(subject),self.node(target),stage,"S" if verdict=="support" else "R",idx))
    def paths(self,hops):
        # All prefix paths are queried through actual native snapshot adjacency; Python is a parity oracle.
        if hops==0:return [(0,"supported",())]
        edges=[e for e in self.edges if e[2]<hops]
        actual=native_paths(self.binary,self.names,edges,hops)
        expected=python_paths(edges,hops)
        if actual!=expected:raise RuntimeError("native/Python graph path divergence")
        return actual
    def frontier(self,hops):
        return sorted({self.names[u] for u,status,_ in self.paths(hops) if status=="supported"},key=normalize)
    def result(self):
        p=self.paths(len(self.plan["relations"]))
        return decide(self.names,p)
def select_windows(subject,relation,ids,corpus,limit=12):
    src=windows(ids,corpus);r=set(normalize(relation).split())
    ranked=sorted(src,key=lambda k:(not mention(subject,src[k]["text"]),-len(r&set(normalize(src[k]["text"]).split())),ids.index(src[k]["doc"]),src[k]["start"],k))
    # Retain all available windows only within fixed budget, prioritise explicit anchor mentions.
    return {k:src[k] for k in ranked[:limit]}
