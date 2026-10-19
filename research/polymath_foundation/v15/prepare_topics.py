#!/usr/bin/env python3
"""Freeze F15 topics into plain native TSV. Country oracle withheld from inference."""
import argparse,csv,hashlib,json,re
from pathlib import Path
QUERIES=[
 ("Q1","Which country does the Garuda airline belong to?"),
 ("Q2","Where is possible gradua airline belongs to?"),
 ("Q3","In which country is Garuda Pancasila a national emblem?"),
 ("Q4","Does a Garuda royal symbol in Thailand establish that Garuda Indonesia is a Thai airline?")
]
FIELDS=["topic_id","title","body","source_url","source_status"]
def generate(topic_file,output):
    docs=json.loads(Path(topic_file).read_text())
    if len(docs)!=20 or len({d["topic_id"] for d in docs})!=20:
        raise ValueError("must contain exactly 20 unique source articles")
    counts={}
    for i,d in enumerate(docs,1):
        if d["topic_id"]!=f"T{i:02}":raise ValueError("unstable topic IDs")
        for field in FIELDS:
            if not isinstance(d.get(field),str) or not d[field]:
                raise ValueError("missing article field")
            if "\t" in d[field] or "\n" in d[field] or "\r" in d[field]:
                raise ValueError("not native TSV-safe")
        n=len(d["body"].split())
        if n<125:raise ValueError("article is not substantive")
        counts[d["topic_id"]]=n
        if d["source_status"]!="project_authored_summary_not_third_party_authenticated":
            raise ValueError("forged third party authentication")
    if docs[18]["topic_id"]!="T19" or not docs[18]["answer_bearing_identity_doc"]:
        raise ValueError("preregistered identity source changed")
    if any(x.get("answer_bearing_identity_doc") for x in docs[:18]+docs[19:]):
        raise ValueError("gold docs leak")
    direct="garuda indonesia is the national flag carrier airline of indonesia"
    if sum(direct in d["body"].lower() for d in docs)!=1:raise ValueError("direct source identity changed")
    output=Path(output)
    output.mkdir(parents=True,exist_ok=True)
    for condition,filtered in [("C0",[d for d in docs if d["topic_id"]!="T19"]),("C1",docs)]:
        with (output/f"{condition}.tsv").open("w",newline="") as h:
            w=csv.DictWriter(h,delimiter="\t",fieldnames=FIELDS,lineterminator="\n")
            w.writeheader();w.writerows([{k:d[k] for k in FIELDS} for d in filtered])
    with (output/"queries.tsv").open("w",newline="") as h:
        w=csv.writer(h,delimiter="\t",lineterminator="\n")
        w.writerow(["query_id","question"]);w.writerows(QUERIES)
    data={"experiment":"MOOL-POLYMATH-F15-001","documents":20,
          "document_words":counts,"total_words":sum(counts.values()),
          "conditions":{"C0":["T19"],"C1":[]},
          "direct_airline_identity_source_for_evaluator_only":"T19",
          "ground_truth_source":"https://www.garuda-indonesia.com/static/en/corporate-partners/company-profile/index.html?lang=en&loc=gb&v=e415e9c3-d734-41cb-8c04-f555274c0bff",
          "expected_answer_country_for_evaluator_only":"Indonesia",
          "evaluator_gold_given_to_native_reasoner":False,
          "source_classification":"author_written_synthesis_not_externally_authenticated",
          "doc_sha256":hashlib.sha256(Path(topic_file).read_bytes()).hexdigest(),
          "file_hashes":{x.name:hashlib.sha256(x.read_bytes()).hexdigest()
            for x in output.glob("*.tsv")}}
    (output/"corpus_manifest.json").write_text(json.dumps(data,sort_keys=True,indent=2)+"\n")
    print(f"F15_CORPUS_FROZEN_PASS 20 topics {sum(counts.values())} words C0=19 C1=20 queries=4")
if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--topics",required=True);ap.add_argument("--out",required=True)
    a=ap.parse_args();generate(a.topics,a.out)
