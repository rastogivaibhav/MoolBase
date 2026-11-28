"""EGPM preregistered split and paired analysis; no model calls or score tuning."""
import argparse, collections, hashlib, json, math, random, re
from pathlib import Path
MANIFEST = '0508c3dac842757eb7cd3249b3c1e64b214f6f6a93441945dc4c9e95037c9dfa'
POPULATION = 'ab03723d9fc5175ecf3f89a647cb5142e279f059e4bc4a85c30f367f81c6c7d2'
DEVELOPMENT = frozenset('2hop__252311_366220 2hop__207476_625987 2hop__390947_232243 2hop__354635_174222 2hop__166824_185403 2hop__683835_709167 2hop__569582_304416 2hop__821197_368148'.split())
SEED = 'EGPM06-v1-frozen-20261126'
PRIMARY = 'D_active_graph'
BASELINES = ('D_active_matched_text', 'B_memo')
def hash_ids(ids):
    return hashlib.sha256(('\n'.join(sorted(ids))+'\n').encode()).hexdigest()
def split(report):
    ids=[r['id'] for r in report['records']]
    if report['manifest']['manifest_sha256']!=MANIFEST or len(ids)!=1000 or len(set(ids))!=1000 or hash_ids(ids)!=POPULATION or not DEVELOPMENT.issubset(ids):
        raise ValueError('wrong benchmark population')
    strata=collections.defaultdict(list)
    for i in set(ids)-DEVELOPMENT:strata[i.split('__')[0]].append(i)
    quota={k:len(v)*100//992 for k,v in strata.items()}
    for k in sorted(strata,key=lambda k:(-(len(strata[k])*100%992),k))[:100-sum(quota.values())]:quota[k]+=1
    h=[];c=[]
    for k,v in sorted(strata.items()):
        v=sorted(v,key=lambda i:(hashlib.sha256((SEED+'|'+i).encode()).hexdigest(),i));n=quota[k]
        h+=v[:n];c+=v[n:2*n]
    if len(h)!=100 or len(c)!=100 or set(h)&set(c) or (set(h)|set(c))&DEVELOPMENT:raise ValueError('invalid split')
    return dict(seed=SEED,manifest=MANIFEST,population=POPULATION,development=sorted(DEVELOPMENT),heldout=sorted(h),confirmation=sorted(c),quota=quota,heldout_sha256=hash_ids(h),confirmation_sha256=hash_ids(c))
def norm(s):return ' '.join(re.findall(r'\w+',s.casefold()))
def is_abstain(s):return norm(s) in ('','no answer','i don t know','i do not know')
def correct(s,aliases):return not is_abstain(s) and norm(s) in {norm(a) for a in aliases}
def f1(s,aliases):
    if is_abstain(s):return 0.
    a=norm(s).split();x=collections.Counter(a)
    return max((2*sum((x&collections.Counter(norm(t).split())).values())/(len(a)+len(norm(t).split())) for t in aliases if a),default=0.)
def paired(diff,strata,seed=20261126,reps=10000):
    if len(diff)!=len(strata) or not diff:raise ValueError('invalid pairs')
    wins=diff.count(1);losses=diff.count(-1);n=wins+losses
    p=sum(math.comb(n,k) for k in range(wins,n+1))/2**n if n else 1.
    groups=collections.defaultdict(list)
    for d,s in zip(diff,strata):groups[s].append(d)
    rng=random.Random(seed);samples=[]
    for _ in range(reps):samples.append(sum(sum(rng.choices(g,k=len(g))) for g in groups.values())/len(diff))
    samples.sort();return dict(delta=sum(diff)/len(diff),wins=wins,losses=losses,p_one_sided=p,ci95=[samples[int(.025*reps)],samples[min(reps-1,int(.975*reps))]])
def analyse(records,frozen,part):
    if part not in ('heldout','confirmation'):raise ValueError('invalid split')
    by={r['id']:r for r in records};ids=frozen[part]
    if len(records)!=len(by) or len(ids)!=100 or set(by)!=set(ids):raise ValueError('missing, duplicate or unexpected evaluation IDs')
    if hash_ids(ids)!=frozen[part+'_sha256']:raise ValueError('split integrity failure')
    arms=(PRIMARY,)+BASELINES;summary={};flags={}
    for a in arms:
        vals=[by[i]['predictions'][a] for i in ids];labels=[by[i]['aliases'] for i in ids]
        flags[a]=[int(correct(v,l)) for v,l in zip(vals,labels)]
        summary[a]=dict(n=len(ids),accuracy=sum(flags[a])/len(ids),mean_f1=sum(f1(v,l) for v,l in zip(vals,labels))/len(ids),coverage=sum(not is_abstain(v) for v in vals)/len(ids),wrong_answer_rate=sum(not is_abstain(v) and not c for v,c in zip(vals,flags[a]))/len(ids))
    comparisons={a:paired([x-y for x,y in zip(flags[PRIMARY],flags[a])],[i.split('__')[0] for i in ids]) for a in BASELINES}
    running=0.
    for j,a in enumerate(sorted(comparisons,key=lambda a:comparisons[a]['p_one_sided'])):
        running=max(running,min(1.,(len(BASELINES)-j)*comparisons[a]['p_one_sided']));comparisons[a]['p_holm']=running
    passed=all(v['delta']>0 and v['ci95'][0]>0 and v['p_holm']<.05 for v in comparisons.values())
    return dict(part=part,primary=PRIMARY,arms=summary,comparisons=comparisons,statistical_gate=passed)
if __name__=='__main__':
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest='cmd',required=True)
    s=sub.add_parser('split');s.add_argument('retrieval_report');s.add_argument('out')
    a=sub.add_parser('analyse');a.add_argument('split');a.add_argument('part',choices=['heldout','confirmation']);a.add_argument('records',nargs='+')
    args=p.parse_args()
    if args.cmd=='split':Path(args.out).write_text(json.dumps(split(json.loads(Path(args.retrieval_report).read_text())),indent=2,sort_keys=True)+'\n')
    else:print(json.dumps(analyse([json.loads(Path(f).read_text()) for f in args.records],json.loads(Path(args.split).read_text()),args.part),indent=2))
