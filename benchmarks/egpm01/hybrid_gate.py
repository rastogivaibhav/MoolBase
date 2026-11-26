"""Trace and evidence gate. No model simulation; native engine required for resolved claims."""
import hashlib,json,subprocess,tempfile
from collections import defaultdict
from pathlib import Path
def trace_index(rows):
    result={}
    for row in rows:
        if not isinstance(row,dict) or not row.get('question_no'): raise ValueError('trace missing question ID')
        qid=str(row['question_no'])
        if qid in result: raise ValueError('duplicate trace ID')
        result[qid]={'question':row['question'],'answer':row.get('model_final_response',''),'error':row.get('error'),'recalls':[{'question':r['question'],'answer':r['answer']} for r in row.get('all_sm_responses_filtered',[])]}
    return result
def fused_ranks(rankings,k=5):
    scores=defaultdict(float)
    for ranking in rankings:
        if len(ranking)!=len(set(ranking)): raise ValueError('duplicate ranked document ID')
        for rank,doc in enumerate(ranking,1): scores[doc]+=1/(60+rank)
    return sorted(scores,key=lambda d:(-scores[d],d))[:k]
def validate_candidate(candidate,docs):
    if not isinstance(candidate,dict) or not isinstance(candidate.get('answer'),str): raise ValueError('invalid answer')
    answer=candidate['answer'].strip()
    if not answer or len(answer.encode())>200: raise ValueError('invalid answer length')
    citations=candidate.get('citations')
    if not isinstance(citations,list) or not 1<=len(citations)<=5: raise ValueError('invalid citation count')
    for cite in citations:
        if not isinstance(cite,dict): raise ValueError('invalid citation')
        d,q=cite.get('id'),cite.get('quote')
        if d not in docs or not isinstance(q,str) or len(q.strip())<8 or q not in docs[d]: raise ValueError('citation not present in retrieved corpus')
    return {'answer':answer,'citations':citations}
def evidence_event(candidate,verdict):
    if verdict not in ('support','refute','unresolved'): raise ValueError('invalid verifier decision')
    if verdict=='unresolved': return None
    digest=hashlib.sha256(json.dumps(candidate['citations'],sort_keys=True).encode()).hexdigest()
    content='External semantic verifier: '+verdict+'; citation chain sha256='+digest
    return ['verified-chain',content,'chain:'+digest[:32],'0',verdict,'1','']
def native_gate(binary,candidate,verdict):
    event=evidence_event(candidate,verdict)
    if event is None: return {'answer':'No answer','status':'unresolved','receipt':None}
    with tempfile.TemporaryDirectory() as tmp:
        path=Path(tmp)/'evidence.tsv'
        path.write_text('\t'.join(event)+'\n',encoding='utf-8')
        result=subprocess.run([str(Path(binary).resolve()),str(Path(tmp)/'db'),str(path),candidate['answer'],'Insufficient evidence'],capture_output=True,text=True,check=True,timeout=60)
        records=[json.loads(line) for line in result.stdout.splitlines() if line.strip()]
        if not records or any(not r.get('ok') for r in records): raise RuntimeError('native runtime did not return valid receipts')
        final=records[-1]; receipt=final['receipt']
        if not receipt.get('coreExecuted') or not receipt.get('noSilentPromotion'): raise RuntimeError('missing runtime evidence contract')
        accept=verdict=='support' and final['status'].lower()=='resolved' and final['answer']==final['targets'][0]['id'] and not final.get('truncated',True)
        return {'answer':candidate['answer'] if accept else 'No answer','status':final['status'],'receipt':final}
