"""Regression tests with synthetic semantic decisions, plus real native path parity."""
import sys,hashlib
from staged_graph import canonical_id,windows,State,safe_alias,valid_plan
b=sys.argv[1];corpus={"docA":"Alice lives in London. London is in England.","docB":"London is not in England.","docC":"London is in Scotland.","docD":"Alicia lives in Paris."}
s=windows(["docA"],corpus);key=next(iter(s))
assert canonical_id("["+key+"]",s)==key
assert canonical_id("["+key+"] forged text",s) is None
assert canonical_id("d999s999",s) is None
assert windows(["docA","docB"],corpus)[key]==s[key]
for v in s.values():assert corpus[v["doc"]][v["start"]:v["end"]]==v["text"] and hashlib.sha256(v["text"].encode()).hexdigest()==v["sha256"]
p={"anchor":"Alice","relations":["lives in","is in"]};assert valid_plan(p,"Where is Alice's home located?");assert not valid_plan(p,"Where is Bob?")
def verify(text,claim):
    if "not in England" in text:return "refute",{"contradiction":1}
    return "support",{"entailment":1}
g=State(p,verify,b);g.add(0,"Alice",[{"subject":"Alice","object":"London","span":"["+key+"]"}],s)
assert g.frontier(1)==["London"]
assert g.result()["status"]=="missing_evidence"
g.add(1,"London",[{"subject":"London","object":"England","span":key}],s)
assert g.result()["answer"]=="England"
g.add(1,"London",[{"subject":"London","object":"England","span":key}],s)
assert len(g.edges)==2
s2=windows(["docB"],corpus);g.add(1,"London",[{"subject":"London","object":"England","span":next(iter(s2))}],s2)
assert g.result()["status"]=="contradictory"
s3=windows(["docD"],corpus);g.add(0,"Alice",[{"subject":"Alicia","object":"Paris","span":next(iter(s3))}],s3)
assert "unverified_alias" in g.errors
assert not safe_alias("Alice","Alicia","Alicia lives in Paris.",verify)[0]
print("EGPM06 regression passed: bracket IDs, forged suffix rejection, stable spans, offsets/hash, plan anchoring, prefix frontier, missing chain, completion, dedupe, contradiction, unverified alias")
