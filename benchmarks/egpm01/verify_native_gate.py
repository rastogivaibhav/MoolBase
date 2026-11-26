#!/usr/bin/env python3
"""Real native integration smoke with explicitly fixture-supplied semantic judgments."""
import json,sys
from hybrid_gate import native_gate,validate_candidate
candidate=validate_candidate({'answer':'London','citations':[{'id':'a','quote':'Alice lives in London.'}]},{'a':'Alice lives in London.'})
results={v:native_gate(sys.argv[1],candidate,v) for v in ('support','refute','unresolved')}
assert results['refute']['answer']=='No answer'
assert results['unresolved']['answer']=='No answer'
assert results['support']['receipt']['receipt']['coreExecuted']
print(json.dumps({v:{'answer':r['answer'],'status':r['status']} for v,r in results.items()},indent=2))
