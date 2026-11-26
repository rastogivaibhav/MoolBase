"""Synthetic labels only; real native graph integration."""
import sys
from candidate_graph import source_windows,grounded_candidates,evaluate_proof,resolve,audit_sources,dual
b=sys.argv[1];c={'a':'Alice lives in London. London is in England.','b':'Alice is not a resident of Paris.'}
s=source_windows(list(c),c);audit_sources(s,c,list(c));sid=next(iter(s))
assert grounded_candidates({'answers':['England','Atlantis','England']},s)==['England']
assert dual('support','refute')=='unresolved' and dual('refute','unresolved')=='unresolved'
raw={'links':[{'subject':'Alice','relation':'lives in','object':'London','span':sid},{'subject':'London','relation':'is in','object':'England','span':sid}]}
q="Where is Alice's home located?"
def nli(t,claim):return 'support',{'entailment':1.0}
def critic(q,c,l,s):return {'first':'support','second':'support','question_coverage':'support'}
def identity(a,b,t):return 'unresolved'
def run(r=raw,crit=critic,candidate='England'):return evaluate_proof(q,candidate,r,s,nli,crit,identity,b)
p=run();assert p['status']=='supported' and p['verifier_only'] and resolve([p])['answer']=='England'
bad=run(crit=lambda *a:{'first':'unresolved','second':'support','question_coverage':'support'});assert bad['status']=='unresolved' and not bad['verifier_only']
bad=run(crit=lambda *a:{'first':'support','second':'support','question_coverage':'unresolved'});assert bad['status']=='unresolved'
bad=run({'links':[raw['links'][0]]});assert bad['prefix']==['London'] and bad['status']=='unresolved'
bad=run({'links':[raw['links'][0],{**raw['links'][1],'span':'fabricated'}]});assert 'invalid_span' in bad['errors']
bad=run({'links':[raw['links'][0],{**raw['links'][1],'subject':'Alice'}]});assert 'disconnected_bridge' in bad['errors']
bad=run(candidate='Paris');assert 'candidate_endpoint_mismatch' in bad['errors']
bad=run({'links':[raw['links'][1],raw['links'][0]]});assert 'anchor_not_in_question' in bad['errors']
assert resolve([p,{**p,'candidate':'Scotland'}])['status']=='ambiguous'
tampered={sid:{**s[sid],'text':'forged'}}
try:audit_sources(tampered,c,list(c))
except ValueError:pass
else:raise AssertionError('forged text accepted')
print('EGPM07 candidate proof smoke passed: grounded candidates, dual disagreement, coverage, prefix, forged ID, disconnected bridge, endpoint, anchor, ambiguity, provenance, native parity')
