"""EGPM13: replay real EGPM12 proposals with blinded binary critic diagnostics.
Development only. No MeMo calls, no QA gold in verifier, no native admission.
"""
import argparse, hashlib, json, time
from collections import Counter
from pathlib import Path
from native_retrieval import prepare
from candidate_graph08 import audit_sources, validate_link, dual
from candidate_eval08 import EXPECTED, NLI_REV
from independent_nli import Verifier
from quant_backend import serve
from quant_eval import obj, structured

SCHEMA = obj({'answer': {'type':'string','enum':['yes','no']},
              'quote': {'type':'string','maxLength':1000}})
# Developer-adjudicated diagnostic sentinels from EGPM12 actual artifacts.
# These are not blind independent labels and never enter the inference prompt.
SENTINELS = {
    'Kevin Smith founded SModcast Pictures.': 'support',
    'North Carolina State University football team plays in Carter-Finley Stadium.': 'support',
    'West Ham United plays for Chelsea.': 'unresolved',
}
# Sentinel ambiguity is intentional: unsupported is not automatically contradicted.
# Their labels are advisory review targets; no automated gate is based on them.

def sha(s):
    return hashlib.sha256(s.encode()).hexdigest()

def quote_span(text, quote, base):
    if not isinstance(quote,str) or not quote.strip():
        return None
    start=text.find(quote)
    if start<0 or text.find(quote,start+1)>=0:
        return None
    return {'start':base+start,'end':base+start+len(quote),'text':quote,'sha256':sha(quote)}

def decision(yes,no):
    # yes/no are the independent answers to entailment and contradiction.
    if yes=='yes' and no=='no':return 'support'
    if yes=='no' and no=='yes':return 'refute'
    return 'unresolved'

def critic_probe(call,premise,claim,base):
    prompts={
        'entailment': 'Does the passage establish the exact claim, including direction, entities and relation? Answer yes only when the claim follows from the passage. A different relation or mere co-occurrence is not enough.',
        'contradiction': 'Does the passage explicitly establish a fact incompatible with the exact claim? Answer yes only for an actual contradiction, not merely missing evidence or a different relationship.'
    }
    out={}
    for kind,question in prompts.items():
        # Never include prior model decisions, QA labels or question text.
        raw,err=call(question+'\nPassage:\n'+premise+'\nClaim:\n'+claim+
            '\nReturn answer yes or no. For yes, quote an exact contiguous passage that justifies it; for no, quote an empty string.',SCHEMA,400)
        val=(raw or {}).get('answer')
        span=quote_span(premise,(raw or {}).get('quote'),base)
        valid=val in ('yes','no') and (span is not None if val=='yes' else (raw or {}).get('quote')=='')
        out[kind]={'raw':raw,'errors':err,'span':span,'answer':val if valid else None}
    return decision(out['entailment']['answer'],out['contradiction']['answer']),out

def selftest():
    assert decision('yes','no')=='support'
    assert decision('no','yes')=='refute'
    assert decision('yes','yes')==decision('no','no')==decision(None,'yes')=='unresolved'
    assert quote_span('Alice founded Acme.','founded Acme',10)['start']==16
    assert quote_span('A A','A',0) is None
    assert quote_span('Alice','Bob',0) is None
    assert quote_span('Alice','',0) is None
    print('PASS: strict binary combination and exact unique quote offsets')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--old',type=Path,required=True)
    p.add_argument('--offset',type=int,choices=range(8),required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    selftest()
    old=json.loads(a.old.read_text())
    rows,corpus,gold,manifest=prepare(120)
    row=rows[a.offset]
    if old['id']!=row['id'] or old['question']!=row['question'] or old['manifest']!=manifest:
        raise ValueError('development source identity drift')
    if manifest['manifest_sha256']!='0508c3dac842757eb7cd3249b3c1e64b214f6f6a93441945dc4c9e95037c9dfa':
        raise ValueError('manifest drift')
    models=json.loads(Path('egpm-models/manifest.json').read_text())
    for k in ('executive_revision','executive_sha256'):
        if models.get(k)!=EXPECTED[k] or old['models'].get(k)!=EXPECTED[k]:
            raise ValueError('executive identity drift')
    verifier=Verifier()
    if verifier.revision!=NLI_REV:raise ValueError('NLI identity drift')
    a.out.parent.mkdir(parents=True,exist_ok=True)
    report={'scope':'development-only frozen proposal replay; not blind precision or QA; no graph insertion',
        'id':row['id'],'input_sha256':hashlib.sha256(a.old.read_bytes()).hexdigest(),
        'manifest':manifest,'models':models,'nli_revision':verifier.revision,'arms':{}}
    def save():a.out.write_text(json.dumps(report,indent=2)+'\n')
    proc=serve(models['executive_path'],18081,'egpm-models/executive.log')
    try:
        for name in ('retrieved','oracle'):
            arm=old['arms'][name]
            source=arm['source'];ids=arm['documents']
            audit_sources(source,corpus,ids)
            if len(arm['proposals'])!=len(arm['audit']):raise ValueError('audit length mismatch')
            result={'records':[],'counts':{},'source':source,'documents':ids}
            report['arms'][name]=result
            counts=Counter()
            for raw,prior in zip(arm['proposals'],arm['audit']):
                counts['proposed']+=1
                link,err=validate_link(raw,source,0)
                if err or prior.get('error'):
                    if err!=prior.get('error') and prior.get('error')!='duplicate':
                        raise ValueError('prior structural audit mismatch')
                    counts[prior.get('error') or err]+=1
                    result['records'].append({'raw':raw,'previous':prior,'error':prior.get('error') or err})
                    continue
                if link['claim']!=prior['claim'] or source[link['span']]!=prior['source']:
                    raise ValueError('prior claim/source drift')
                span=source[link['span']];premise=span['text'];claim=link['claim']
                # Detect rather than silently accept truncation.
                n_tokens=len(verifier.tokenizer(premise,claim,truncation=False)['input_ids'])
                v,probs=verifier(premise,claim)
                counts['nli_'+v]+=1
                calls=[]
                def call(prompt,schema,cap):
                    t=time.monotonic()
                    raw,errors=structured(18081,prompt,schema,cap)
                    calls.append({'seconds':time.monotonic()-t,'prompt_chars':len(prompt),'cap':cap,'errors':errors})
                    return raw,errors
                cv,details=critic_probe(call,premise,claim,span['start'])
                verdict=dual(v,cv) if n_tokens<=512 else 'unresolved'
                counts['critic_'+cv]+=1
                counts['dual_'+verdict]+=1
                counts['previous_'+prior['verdict']]+=1
                if n_tokens>512:counts['truncated_nli']+=1
                item={'claim':claim,'link':link,'source':span,'previous':prior,
                    'nli':v,'probabilities':probs,'nli_tokens':n_tokens,
                    'critic':cv,'critic_details':details,'verdict':verdict,'calls':calls}
                if claim in SENTINELS:item['developer_review_hint']=SENTINELS[claim]
                result['records'].append(item)
            result['counts']=dict(counts);save()
            print(name,json.dumps(result['counts']),flush=True)
    finally:
        proc.terminate()
        try:proc.wait(timeout=10)
        except Exception:proc.kill();proc.wait()
    # Generate reviewer rows without old/new verdicts, model scores or gold answers.
    review=[]
    for name,arm in report['arms'].items():
        for item in arm['records']:
            if 'claim' not in item:continue
            identity=sha(item['claim']+'\0'+item['source']['doc']+'\0'+str(item['source']['start']))
            review.append({'review_id':identity,'claim':item['claim'],'source':item['source'],
                           'label':None,'reason':None})
    # Same claim/span across arms is not independent evidence.
    unique={x['review_id']:x for x in review}
    a.out.with_suffix('.review.jsonl').write_text(''.join(json.dumps(unique[k])+'\n' for k in sorted(unique)))

if __name__=='__main__':main()
