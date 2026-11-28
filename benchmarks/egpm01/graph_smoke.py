import sys
from graph_adapter import spans,materialize,python_paths,native_paths,decide
binary=sys.argv[1]
names=["A","B","C","D"]
cases=[
 ("positive",[(0,1,0,"S",0),(1,2,1,"S",1)],"resolved","C"),
 ("missing",[(0,1,0,"S",0)],"missing_evidence","No answer"),
 ("refuted",[(0,1,0,"S",0),(1,2,1,"R",1)],"refuted_chain","No answer"),
 ("contradictory",[(0,1,0,"S",0),(1,2,1,"S",1),(1,2,1,"R",2)],"contradictory","No answer"),
 ("competing",[(0,1,0,"S",0),(1,2,1,"S",1),(1,3,1,"S",2)],"ambiguous","No answer"),
 ("wrong_direction",[(1,0,0,"S",0),(1,2,1,"S",1)],"missing_evidence","No answer"),
 ("wrong_stage",[(0,1,1,"S",0),(1,2,0,"S",1)],"missing_evidence","No answer"),
]
for label,edges,status,answer in cases:
    a=native_paths(binary,names,edges,2);b=python_paths(edges,2);assert a==b,(label,a,b)
    d=decide(names,a);assert (d["status"],d["answer"])==(status,answer),(label,d)
    print(label,d["status"],d["answer"])
s=spans(["a"],{"a":"Alice lives in London. London is in England."})
for v in s.values():assert v["text"]=="Alice lives in London. London is in England."[v["start"]:v["end"]]
plan={"anchor":"Alice","relations":["lives in","is in"]}
rows=[{"stage":0,"subject":"Alice","object":"London","span":"d0s0"},{"stage":1,"subject":"London","object":"England","span":"d0s1"},{"stage":1,"subject":"London","object":"Paris","span":"d0s1"}]
n,e,a,errors=materialize(plan,rows,s,lambda x,y:("support",{"entailment":1}))
assert len(e)==2 and errors==["entity_not_in_span"]
assert decide(n,native_paths(binary,n,e,2))["answer"]=="England"
print("8 native/offset/provenance cases passed; synthetic verification labels only")
