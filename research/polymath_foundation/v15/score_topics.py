#!/usr/bin/env python3
"""Score evidence availability and retrieval, NOT model-generated country answers."""
import argparse,csv,json,math,re
from pathlib import Path
from collections import Counter
STOP={"where","is","possible","the","a","an","does","in","which","what","are","of","to","this","that","it","country","belongs","belong","do"}
TOPICS={"Q1":"T19","Q2":"T19","Q3":"T12","Q4":"T19"}
QTRUTH={"Q1":"Indonesia","Q2":"Indonesia","Q3":"Indonesia","Q4":"No; Thai royal symbolism alone does not establish airline origin"}
def tsv(path):
    with open(path,newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
def terms(s):
    return [x for x in re.findall(r"[a-z]{3,}",s.lower()) if x not in STOP]
def lexical(topics,q,k=5):
    t=terms(q);scores=[]
    for topic in topics:
        words=Counter(terms(topic["title"]+" "+topic["body"]))
        title=Counter(terms(topic["title"]))
        # Fixed, preregistered independent purely lexical comparison.
        score=sum(min(words.get(x,0),3)+2*min(title.get(x,0),2) for x in t)
        scores.append((score,topic["topic_id"]))
    return [y for x,y in sorted(scores,key=lambda a:(-a[0],a[1]))[:k]]
def score(raw,documents,queries,manifest):
    assert manifest["documents"]==20
    expected={(c,q,a) for c in ("C0","C1") for q in ("Q1","Q2","Q3","Q4") for a in ("VECTOR","G0","G1","G2")}
    got={}
    for r in raw:
        key=(r.get("corpus"),r.get("query_id"),r.get("arm"))
        if key not in expected or key in got:raise ValueError("invalid or duplicate native condition")
        got[key]=r
        if r["document_count"]!=(19 if key[0]=="C0" else 20):raise ValueError("missing corpus topic")
        if r["text_query"]!=queries[key[1]]:raise ValueError("question changed")
        if key[2]=="VECTOR":
            if r.get("result_is_natural_language_answer") is not False or len(r["top5"])!=5:
                raise ValueError("vector retrieval misrepresented as model answer")
            if any(not math.isfinite(x["score"]) for x in r["top5"]):
                raise ValueError("vector returned nonfinite relevance")
            if len({x["topic_id"] for x in r["top5"]})!=5:
                raise ValueError("duplicate node search result")
        else:
            if (r.get("answer_generation_implemented") or r.get("scientific_claim_verified") or
                r.get("source_authentication_performed") or r.get("model_world_updated") or
                r.get("learned_weights_updated")):
                raise ValueError("authored corpus promoted to verified learned knowledge")
            if r["native_graph_edges"]!=0 or r["actual_opposition_executed"]!=(key[2]=="G2"):
                raise ValueError("pre-authored cross-topic graph links or invalid runtime arm")
            if r["selected_topic"]!="none" and r["selected_topic"] not in {d["topic_id"] for d in documents[key[0]]}:
                raise ValueError("native selected an article not present in corpus")
            if r["elapsed_ms"]<0 or not math.isfinite(r["elapsed_ms"]):
                raise ValueError("invalid native latency")
    if set(got)!=expected:raise ValueError("native experiment missing expected conditions")
    result=[]
    for c in ("C0","C1"):
        topic_ids={x["topic_id"] for x in documents[c]}
        for q in ("Q1","Q2","Q3","Q4"):
            target=TOPICS[q]
            vec=[x["topic_id"] for x in got[c,q,"VECTOR"]["top5"]]
            lex=lexical(documents[c],queries[q])
            native={}
            for arm in ("G0","G1","G2"):
                r=got[c,q,arm]
                native[arm]={"status":r["native_status"],
                  "has_selected_document":r["has_selected_document"],
                  "selected_topic":r["selected_topic"],
                  "target_document_selected":r["has_selected_document"] and r["selected_topic"]==target,
                  "material_challenge":r["material_challenge"],
                  "reopen_requested":r["reopen_requested"],
                  "candidate_count":r["semantic_candidate_count"],
                  "elapsed_ms":r["elapsed_ms"]}
            result.append({"corpus":c,"query_id":q,"query_text":queries[q],
                "answer_for_evaluator_only":QTRUTH[q],
                "direct_identity_evidence_present":target in topic_ids,
                "required_target_document":target,
                "vector_top5":vec,"lexical_top5":lex,
                "vector_retrieved_target":target in vec,
                "lexical_retrieved_target":target in lex,
                "native":native})
    return {"experiment":"MOOL-POLYMATH-F15-001","topics":20,
       "source_words":manifest["total_words"],
       "genuine_native_calls":24,"native_vector_queries":8,
       "model_generated_country_answers":0,
       "ground_truth_used_during_inference":False,
       "external_source_authentication":"NOT_RUN",
       "independent_scientific_associative_learning":"NOT_ESTABLISHED",
       "direct_evidence_country_answer":"Indonesia",
       "knowledge_gap_answer_policy":"ABSTAIN_NO_CORPORATE_COUNTRY_SOURCE",
       "scenario_results":result,
       "limitations":["Article summaries written for this test; URLs are contextual references, not authenticated ingestion.",
        "The native runtime produces candidate document IDs and governed status, not free-form natural-language answers.",
        "The test inserts isolated topic nodes, not auto-generated cross-topic causal links. Zero edges is a baseline behaviour, not proof that no other graph pipeline could make them.",
        "Retrieval of a relevant document is not equivalent to correctly reading or reasoning over its content.",
        "Country names shared with Hindu and Thai cultural contexts do not prove airline nationality.",
        "No weight updates, feedback outcomes, long-term learning or independent blind topic authoring were performed."]}
def main():
    p=argparse.ArgumentParser()
    for name in ("native","c0","c1","questions","manifest","output"):
        p.add_argument("--"+name,required=True)
    a=p.parse_args()
    records=[json.loads(x) for x in Path(a.native).read_text().splitlines() if x.strip()]
    docs={"C0":tsv(a.c0),"C1":tsv(a.c1)}
    qs={x["query_id"]:x["question"] for x in tsv(a.questions)}
    out=score(records,docs,qs,json.loads(Path(a.manifest).read_text()))
    Path(a.output).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")
    for r in out["scenario_results"]:
        print(f"F15 {r['corpus']} {r['query_id']} source_present={r['direct_identity_evidence_present']} "
              f"vector_has_evidence={r['vector_retrieved_target']} lexical_has_evidence={r['lexical_retrieved_target']} "
              f"G1={r['native']['G1']['status']} G2={r['native']['G2']['status']}")
    print("F15_NATIVE_AND_RETRIEVAL_SCORED=PASS 8 controlled queries 24 native runs")
if __name__=="__main__":main()
