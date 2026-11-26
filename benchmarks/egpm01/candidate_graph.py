"""EGPM07: candidate-conditioned two-hop proof search, dual verification and native parity.
Question-specific research adapter, not a new database core feature.
"""
import hashlib,re
from staged_graph import canonical_id,mention
from graph_adapter import normalize,native_paths,python_paths
def source_windows(ids,corpus,limit=1400):
    out={}
    for doc in dict.fromkeys(ids):
        text=corpus[doc]
        # Preserve titles and neighboring sentences in short source paragraphs.
        for start in range(0,len(text),limit-300):
            end=min(len(text),start+limit)
            t=text[start:end].strip()
            if t:
                a=start+len(text[start:end])-len(text[start:end].lstrip());z=a+len(t)
                key='e'+hashlib.sha256((doc+'\0'+str(a)+'\0'+str(z)).encode()).hexdigest()[:12]
                v=dict(doc=doc,start=a,end=z,text=t,sha256=hashlib.sha256(t.encode()).hexdigest())
                if key in out and out[key]!=v:raise RuntimeError("source ID collision")
                out[key]=v
            if end==len(text):break
    return out
def select_source(ids,corpus,q,candidate='',budget=11000):
    s=source_windows(ids,corpus);terms=set(normalize(q+' '+candidate).split())
    ranked=sorted(s,key=lambda k:(-int(bool(candidate) and mention(candidate,s[k]['text'])),-len(terms&set(normalize(s[k]['text']).split())),ids.index(s[k]['doc']),s[k]['start']))
    chosen={};size=0
    for k in ranked:
        n=len(s[k]['text'])+len(k)+4
        if size+n>budget:continue
        chosen[k]=s[k];size+=n
    return chosen
def grounded_candidates(raw,source):
    out=[];seen=set()
    for c in (raw or {}).get('answers',[]):
        if not isinstance(c,str) or not normalize(c) or len(c.encode())>200 or normalize(c) in seen or not any(mention(c,v['text']) for v in source.values()):continue
        seen.add(normalize(c));out.append(c.strip())
    return out[:3]
def align(a,b,source,nli,identity_critic):
    if normalize(a)==normalize(b):return True,None
    for sid,v in source.items():
        if not (mention(a,v['text']) and mention(b,v['text'])):continue
        claim=a+' is the same entity as '+b+'.'
        verdict,p=nli(v['text'],claim)
        if verdict=='support' and identity_critic(a,b,v['text'])=='support':
            return True,dict(span=sid,claim=claim,probabilities=p)
    return False,None
def validate_link(raw,source,stage):
    if not isinstance(raw,dict):return None,'invalid_link'
    sid=canonical_id(raw.get('span'),source);a=raw.get('subject');b=raw.get('object');rel=raw.get('relation')
    if sid is None:return None,'invalid_span'
    if not all(isinstance(x,str) and normalize(x) for x in (a,b,rel)) or len(rel)>100 or max(len(a.encode()),len(b.encode()))>200 or normalize(a)==normalize(b):return None,'invalid_endpoint_or_relation'
    text=source[sid]['text']
    if not (mention(a,text) and mention(b,text)):return None,'endpoint_not_in_source'
    return dict(stage=stage,subject=a,object=b,relation=rel,span=sid,claim=a+' '+rel+' '+b+'.'),None
def dual(nli,critic):
    # A classifier disagreement or absence of evidence is never promoted to contradiction.
    return nli if nli==critic and nli in ('support','refute') else 'unresolved'
def evaluate_proof(q,candidate,raw,source,nli,critic,identity_critic,binary):
    errors=[];links=[];aliases=[];prefix=[]
    for stage,item in enumerate((raw or {}).get('links',[])[:2]):
        link,err=validate_link(item,source,stage)
        if err:errors.append(err);break
        if stage==0 and not mention(link['subject'],q):errors.append('anchor_not_in_question');break
        if stage==1:
            ok,alias=align(links[0]['object'],link['subject'],source,nli,identity_critic)
            if not ok:errors.append('disconnected_bridge');break
            if alias:aliases.append(alias)
        links.append(link)
    endpoint_ok=False
    if len(links)==2:
        endpoint_ok,alias=align(links[1]['object'],candidate,source,nli,identity_critic)
        if alias:aliases.append(alias)
        if not endpoint_ok:errors.append('candidate_endpoint_mismatch')
    critique=critic(q,candidate,links,source) if links else {}
    for i,link in enumerate(links):
        v,p=nli(source[link['span']]['text'],link['claim'])
        cv=(critique or {}).get('first' if i==0 else 'second','unresolved')
        link.update(nli=v,probabilities=p,critic=cv,verdict=dual(v,cv))
    if links and links[0]['verdict']=='support':prefix=[links[0]['object']]
    # Distinct candidate proofs are kept separate: no joins between incompatible relations.
    names=[links[0]['subject'] if links else 'anchor',links[0]['object'] if links else 'bridge',candidate]
    edges=[]
    for i,link in enumerate(links):
        if i==1 and not endpoint_ok:continue
        if link['verdict'] in ('support','refute'):edges.append((i,i+1,i,'S' if link['verdict']=='support' else 'R',i))
    # Refuse a repeated node/alias cycle even if model proposed a structurally two-hop chain.
    if len(set(map(normalize,names)))!=3:
        errors.append('cycle_or_duplicate_entity');edges=[]
    native=native_paths(binary,names,edges,2)
    reference=python_paths(edges,2)
    if native!=reference:raise RuntimeError('native/Python path mismatch')
    covered=(critique or {}).get('question_coverage')=='support'
    verifier_ok=len(links)==2 and endpoint_ok and covered and all(l['verdict']=='support' for l in links) and len(set(map(normalize,names)))==3
    graph_ok=verifier_ok and any(u==2 and status=='supported' for u,status,_ in native)
    status='supported' if graph_ok else 'refuted_chain' if any(p[1]=='refuted_chain' for p in native) else 'unresolved'
    return dict(candidate=candidate,status=status,links=links,aliases=aliases,critic=critique,errors=errors,names=names,edges=edges,paths=native,verifier_only=verifier_ok,prefix=prefix)
def resolve(proofs,verifier_only=False):
    supported={normalize(p['candidate']):p['candidate'] for p in proofs if (p['verifier_only'] if verifier_only else p['status']=='supported')}
    # Only contradictory verdicts about the same canonical directed claim are conflicts.
    signs={}
    for p in proofs:
        for l in p['links']:
            if l['verdict'] in ('support','refute'):
                key=normalize(l['claim']);signs.setdefault(key,set()).add(l['verdict'])
    contested={k for k,v in signs.items() if len(v)>1}
    disputed=any(normalize(l['claim']) in contested for p in proofs if p['status']=='supported' for l in p['links'])
    status='contradictory' if disputed else 'ambiguous' if len(supported)>1 else 'resolved' if len(supported)==1 else 'refuted_chain' if any(p['status']=='refuted_chain' for p in proofs) else 'missing_evidence'
    return dict(answer=next(iter(supported.values())) if status=='resolved' else 'No answer',status=status,candidates=sorted(supported.values()),contested_claims=sorted(contested))
def audit_sources(source,corpus,ids):
    for sid,v in source.items():
        d,a,z=v['doc'],v['start'],v['end']
        if d not in ids or d not in corpus or not 0<=a<z<=len(corpus[d]) or corpus[d][a:z]!=v['text'] or hashlib.sha256(v['text'].encode()).hexdigest()!=v['sha256'] or sid!='e'+hashlib.sha256((d+'\0'+str(a)+'\0'+str(z)).encode()).hexdigest()[:12]:raise ValueError('source provenance mismatch: '+sid)
