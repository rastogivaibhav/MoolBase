"""Research-only atomic admission; no answer, anchor, or gold input.
Verifier callbacks must be supplied by the caller; this module does not certify semantics.
"""
import hashlib
from candidate_graph08 import audit_sources, validate_link, dual

def compile_edges(proposals, source, corpus, ids, nli, critic):
    audit_sources(source, corpus, ids)
    accepted, audit, seen = [], [], set()
    for raw in proposals:
        link, error = validate_link(raw, source, 0)
        if error:
            audit.append(dict(raw=raw, error=error))
            continue
        span = source[link['span']]
        key = (link['claim'], span['doc'], span['start'], span['end'])
        if key in seen:
            audit.append(dict(raw=raw, error='duplicate'))
            continue
        seen.add(key)
        verdict, probabilities = nli(span['text'], link['claim'])
        second = critic(span['text'], link['claim'])
        result = dict(link, source=dict(span), nli=verdict, critic=second,
                      probabilities=probabilities, verdict=dual(verdict, second))
        audit.append(result)
        if result['verdict'] in ('support', 'refute'):
            accepted.append(result)
    return accepted, audit

def selftest():
    from candidate_graph08 import source_windows
    corpus = {'d': 'Alice founded Acme. Bob lives in York.'}
    source = source_windows(['d'], corpus)
    sid = next(iter(source))
    good = dict(subject='Alice', relation='founded', object='Acme', span=sid)
    other = dict(subject='Bob', relation='lives in', object='York', span=sid)
    bad = dict(good, span='invented')
    absent = dict(good, object='Unknown')
    # Stub verdicts exercise admission plumbing only, not semantic accuracy.
    nli = lambda premise, claim: ('support', {})
    critic = lambda premise, claim: 'support'
    edges, audit = compile_edges([good, other, bad, absent, good], source, corpus, ['d'], nli, critic)
    assert len(edges) == 2 and len(audit) == 5
    assert [a.get('error') for a in audit[2:]] == ['invalid_span','endpoint_not_in_source','duplicate']
    assert not compile_edges([good], source, corpus, ['d'], nli, lambda *_: 'refute')[0]
    for patch in ({'sha256':'bad'}, {'start':1}, {'doc':'unknown'}, {'text':'forged'}):
        corrupted = {sid:dict(source[sid], **patch)}
        try:
            compile_edges([good], corrupted, corpus, ['d'], nli, critic)
        except ValueError:
            pass
        else:
            raise AssertionError('corrupted source accepted')
    print('PASS: disconnected atomic claims retained; unknown span, absent endpoint, duplicate, disagreement and four corrupted source cases rejected; stub semantics only')

if __name__ == '__main__':
    selftest()
