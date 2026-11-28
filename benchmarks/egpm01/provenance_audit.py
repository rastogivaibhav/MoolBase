"""Fail-closed audit of EGPM06+ graph records against an independently supplied corpus."""
import hashlib,re

def norm(s):return ' '.join(re.findall(r'\w+',s.casefold()))
def sha(s):return hashlib.sha256(s.encode()).hexdigest()
def audit_arm(arm,plan,corpus):
    errors=[];spans=arm.get('spans',{});entries=arm.get('audit',[]);names=arm.get('names',[]);edges=arm.get('edges',[]);retrieved=set(arm.get('retrieved',[]));paths=arm.get('paths',[]);relations=(plan or {}).get('relations',[])
    def require(ok,msg):
        if not ok:errors.append(msg)
    for sid,v in spans.items():
        try:
            d,a,z,t=v['doc'],v['start'],v['end'],v['text']
            require(d in retrieved and d in corpus and type(a)==type(z)==int and 0<=a<z<=len(corpus[d]) and corpus[d][a:z]==t and sha(t)==v['sha256'] and sid=='e'+sha(d+'\0'+str(a)+'\0'+str(z))[:12],f'bad span {sid}')
        except (KeyError,TypeError,ValueError):errors.append(f'malformed span {sid}')
    for i,e in enumerate(entries):
        try:
            sid=e['span'];stage=e['stage'];subject=e['subject'];surface=e['surface'];target=e['object'];v=e['verdict'];p=e['probabilities'];t=spans[sid]['text']
            require(type(stage)==int and 0<=stage<len(relations) and e['claim']==surface+' '+relations[stage]+' '+target+'.',f'bad claim {i}')
            require(all(norm(x) and ' '+norm(x)+' ' in ' '+norm(t)+' ' for x in (surface,target)),f'unsupported endpoint {i}')
            require(norm(surface)==norm(subject) or (' '+norm(subject)+' ' in ' '+norm(t)+' ' and e['alias_probabilities']['entailment']>=.8),f'bad alias {i}')
            require(v in ('support','refute','unresolved') and (v!='support' or p['entailment']>=.8) and (v!='refute' or p['contradiction']>=.8),f'bad verdict {i}')
        except (KeyError,TypeError,ValueError,IndexError):errors.append(f'malformed audit {i}')
    valid={}
    for e in edges:
        try:
            u,v,stage,sign,i=e;a=entries[i]
            ok=all(type(k)==int and k>=0 for k in (u,v,stage,i)) and u<len(names) and v<len(names) and i<len(entries) and i not in valid and norm(names[u])==norm(a['subject']) and norm(names[v])==norm(a['object']) and stage==a['stage'] and (sign,a['verdict']) in (('S','support'),('R','refute'))
            require(ok,f'bad edge {e}')
            if ok:valid[i]=e
        except (KeyError,TypeError,ValueError,IndexError):errors.append(f'malformed edge {e}')
    accepted=0
    for path in paths:
        try:
            endpoint,status,indices=path;u=0;seen={0};signs=[];contested=False
            for stage,i in enumerate(indices):
                x,y,s,sign,_=valid[i];require(x==u and s==stage and y not in seen,f'broken path {indices}');u=y;seen.add(y);signs.append(sign)
                contested|=len({e[3] for e in valid.values() if e[:3]==[x,y,s] or tuple(e[:3])==(x,y,s)})>1
            expected='contested' if contested else 'refuted_chain' if 'R' in signs else 'supported'
            require(len(indices)==len(relations) and names[u]==endpoint and status==expected,f'bad path {indices}')
            if status=='supported' and endpoint==arm.get('answer'):accepted+=1
        except (KeyError,TypeError,ValueError,IndexError):errors.append(f'malformed path {path}')
    if arm.get('status')=='resolved':require(arm.get('answer') not in ('','No answer',None) and accepted>0,'resolved without supported complete path')
    else:require(arm.get('answer')=='No answer','nonresolved returned answer')
    return {'ok':not errors,'errors':errors,'spans':len(spans),'verified_edges':len(valid),'paths':len(paths),'accepted_answer_paths':accepted}
