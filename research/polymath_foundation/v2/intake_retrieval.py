#!/usr/bin/env python3
"""Polymath Foundation v2 -- external-source *paraphrase* intake and retrieval.

No network fetch, no LLM, no production DB mutations, no claim of source
authentication or domain comprehension. Standard library only.
"""
import argparse
import collections
import hashlib
import json
import math
import re
from pathlib import Path
from urllib.parse import urlsplit

STOP = frozenset(
    "a an and are as at be by can do does for from how in into is it of on "
    "or that the their these this through to under what when where which who "
    "why with without after before should there rather than all new"
    .split()
)
ALLOWED_DOMAINS = {"networks", "biology", "supply_chain"}
PERMITTED_RIGHTS = {"review_required", "not_relicensed"}


class IntakeError(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise IntakeError(reason)


def nonblank(value, context):
    require(isinstance(value, str) and bool(value.strip()),
            context + ": required nonblank string")
    return value


def text_tokens(s):
    return [x for x in re.findall(r"[a-z0-9]+", s.lower())
            if x not in STOP and len(x) > 1]


def validate_collection(collection):
    require(isinstance(collection, dict), "intake document must be object")
    require(collection.get("schema_version") == "polymath-foundation-2",
            "unsupported collection schema")
    require(collection.get("source_use") ==
            "citation_and_author_paraphrase_only",
            "do not ingest external copyrighted source text or unknown rights")
    require(collection.get("collection") ==
            "external_source_paraphrase_notes", "unsupported collection kind")
    nonblank(collection.get("accessed_date"), "accessed_date")
    sources = collection.get("sources")
    notes = collection.get("notes")
    require(isinstance(sources, list) and len(sources) >= 6,
            "at least six documented external sources required")
    require(isinstance(notes, list) and len(notes) >= 12,
            "at least twelve individually attributed notes required")
    registry = {}
    for source in sources:
        require(isinstance(source, dict), "source must be record")
        sid = nonblank(source.get("id"), "source.id")
        require(sid not in registry, "duplicate source id: " + sid)
        require(source.get("domain") in ALLOWED_DOMAINS,
                "invalid source domain: " + sid)
        for field in ("title", "publisher", "section", "source_family",
                      "publication_date"):
            nonblank(source.get(field), "source." + sid + "." + field)
        url = nonblank(source.get("url"), "source.url")
        parsed = urlsplit(url)
        require(parsed.scheme == "https" and bool(parsed.netloc) and
                not parsed.username and not parsed.password,
                "external source must be fully qualified public HTTPS URL")
        require(source.get("license_status") in PERMITTED_RIGHTS,
                "invalid license posture / claims of free reuse")
        registry[sid] = source

    note_map = {}
    count_by_domain = collections.Counter()
    for note in notes:
        require(isinstance(note, dict), "note must be object")
        nid = nonblank(note.get("id"), "note.id")
        require(nid not in note_map, "duplicate note: " + nid)
        sid = nonblank(note.get("source_id"), "note.source_id")
        require(sid in registry, "dangling source reference: " + sid)
        domain = note.get("domain")
        require(domain == registry[sid]["domain"],
                "note/source domain conflict: " + nid)
        cid = nonblank(note.get("concept_id"), "note.concept_id")
        require(cid.startswith(domain + ":"), "concept identity not domain-qualified")
        nonblank(note.get("label"), "note.label")
        nonblank(note.get("summary"), "note.summary")
        nonblank(note.get("source_anchor"), "note.source_anchor")
        require(note.get("source_anchor") == registry[sid]["section"],
                "citation anchor not registered: " + nid)
        require(note.get("content_form") == "author_paraphrase",
                "publisher text must not be mislabelled as authored content")
        require(note.get("verification_status") == "human_review_needed",
                "no accepted source is autonomously verified")
        require(note.get("allow_model_training") is False,
                "model-training permission not established")
        for field in ("roles", "assumptions", "boundaries"):
            values = note.get(field)
            require(isinstance(values, list) and len(values) > 0 and
                    all(isinstance(x, str) and x.strip() for x in values) and
                    len(values) == len(set(values)),
                    "note missing/distorted " + field + ": " + nid)
        # A note may resemble a claim but does not inherit source authority.
        require(not any(k in note for k in
                        ("verified", "ground_truth", "authority_override")),
                "attempted epistemic status laundering: " + nid)
        note_map[nid] = note
        count_by_domain[domain] += 1
    require(all(count_by_domain[d] >= 3 for d in ALLOWED_DOMAINS),
            "each domain needs at least three independent source notes")
    return registry, note_map


def validate_questions(questions, notes):
    require(questions.get("experiment") == "MOOL-POLYMATH-F2-001",
            "invalid preregistered experiment")
    require(questions.get("evaluation_kind") ==
            "open_authored_retrieval_targets_not_blind", "do not claim blind gold")
    rows = questions.get("questions")
    require(isinstance(rows, list) and len(rows) >= 12, "insufficient questions")
    require(questions.get("question_count") == len(rows), "count mismatch")
    seen = set()
    for q in rows:
        qid = nonblank(q.get("id"), "question.id")
        require(qid not in seen, "duplicate question")
        seen.add(qid)
        nonblank(q.get("text"), "question.text")
        require(q.get("domain") in ALLOWED_DOMAINS | {None},
                "question unknown domain")
        tags = q.get("required_roles")
        require(isinstance(tags, list) and len(tags) == len(set(tags)) and
                all(isinstance(x, str) and x.strip() for x in tags),
                "question invalid controlled roles")
        target = q.get("gold_note_ids")
        require(isinstance(target, list) and len(target) == len(set(target)),
                "question duplicated or malformed gold targets")
        require(all(g in notes for g in target),
                "question points to absent source evidence")
    return rows


def tfidf_index(note_map):
    # Deliberately weak baseline; no embeddings, unrecorded reranking or LM.
    doc_tokens = {}
    document_frequency = collections.Counter()
    for nid, note in note_map.items():
        tokens = text_tokens(note["label"] + " " + note["summary"])
        freq = collections.Counter(tokens)
        doc_tokens[nid] = freq
        document_frequency.update(freq.keys())
    count = len(note_map)
    idf = {word: math.log((1 + count) / (1 + frequency)) + 1
           for word, frequency in document_frequency.items()}
    vectors = {}
    norms = {}
    for nid, tokens in doc_tokens.items():
        vec = {word: freq * idf[word] for word, freq in tokens.items()}
        vectors[nid] = vec
        norms[nid] = math.sqrt(sum(v * v for v in vec.values()))
    return vectors, norms, idf


def search(q, mode, note_map, registry, index, k=3):
    require(mode in ("B0", "B1"), "unknown retrieval baseline")
    vectors, norms, idf = index
    query_tokens = collections.Counter(text_tokens(q["text"]))
    query_vec = {word: f * idf[word] for word, f in query_tokens.items()
                 if word in idf}
    query_norm = math.sqrt(sum(x*x for x in query_vec.values()))
    if not query_norm:
        return {"status": "abstain_no_shared_tokens", "results": []}
    rows = []
    for nid, note in note_map.items():
        overlap = sum(query_vec.get(w, 0.0) * v
                      for w, v in vectors[nid].items())
        if overlap <= 0:
            continue  # no invented relevance from domain/role priors
        base = overlap / (query_norm * norms[nid]) if norms[nid] else 0.0
        score = base
        if mode == "B1":
            if q.get("domain") == note["domain"]:
                score += 0.15
            score += min(0.20, 0.10 * len(
                set(q["required_roles"]) & set(note["roles"])))
        source = registry[note["source_id"]]
        rows.append({"note_id": nid, "score": round(score, 12),
                     "source_id": note["source_id"],
                     "source_family": source["source_family"],
                     "citation_url": source["url"],
                     "citation_anchor": note["source_anchor"],
                     "epistemic_status": "paraphrase_needs_human_review"})
    rows.sort(key=lambda v: (-v["score"], v["note_id"]))
    if mode == "B1":
        chosen = []
        families = set()
        for row in rows:
            if row["source_family"] in families:
                continue
            families.add(row["source_family"])
            chosen.append(row)
            if len(chosen) == k:
                break
        rows = chosen
    else:
        rows = rows[:k]
    return {"status": "retrieved_unverified_notes" if rows else
            "abstain_no_shared_tokens", "results": rows}


def evaluate(collection, questions):
    sources, notes = validate_collection(collection)
    rows = validate_questions(questions, notes)
    index = tfidf_index(notes)
    report = {"experiment": "MOOL-POLYMATH-F2-001",
              "status": "DESCRIPTIVE_EVIDENCE_RETRIEVAL_ONLY",
              "domain_comprehension": "NOT_ASSESSED",
              "external_text_training": False,
              "source_authentication": "NOT_ESTABLISHED",
              "source_count": len(sources), "note_count": len(notes),
              "question_count": len(rows), "methods": {}}
    for method in ("B0", "B1"):
        results = []
        for q in rows:
            found = search(q, method, notes, sources, index)
            gold = q["gold_note_ids"]
            predicted = [e["note_id"] for e in found["results"]]
            positions = [predicted.index(g)+1 for g in gold if g in predicted]
            reciprocal = (1 / min(positions)) if positions else 0.0
            results.append({
                "question_id": q["id"], "domain": q["domain"],
                "out_of_corpus": not bool(gold), "result": found,
                "target_note_ids": gold,
                "recall_at_3": float(bool(positions)) if gold else None,
                "reciprocal_rank": reciprocal if gold else None,
                "abstained": found["status"].startswith("abstain")
            })
        applicable = [v for v in results if not v["out_of_corpus"]]
        out_of_corpus = [v for v in results if v["out_of_corpus"]]
        report["methods"][method] = {
            "in_corpus_n": len(applicable),
            "out_of_corpus_n": len(out_of_corpus),
            "recall_at_3": sum(r["recall_at_3"] for r in applicable)/len(applicable),
            "mrr_at_3": sum(r["reciprocal_rank"] for r in applicable)/len(applicable),
            "ood_abstention_rate": sum(r["abstained"] for r in out_of_corpus) /
                                    len(out_of_corpus) if out_of_corpus else None,
            "per_domain": {d: {
                "n": len(v := [r for r in applicable if r["domain"] == d]),
                "recall_at_3": sum(r["recall_at_3"] for r in v)/len(v) if v else None
            } for d in ("networks", "biology", "supply_chain")},
            "traces": results
        }
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sources", required=True)
    p.add_argument("--questions", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    a = Path(args.sources)
    b = Path(args.questions)
    try:
        doc = json.loads(a.read_text(encoding="utf-8"))
        questions = json.loads(b.read_text(encoding="utf-8"))
        report = evaluate(doc, questions)
    except (OSError, ValueError, KeyError, TypeError, ZeroDivisionError) as e:
        p.exit(2, "foundation_v2_invalid: " + str(e) + "\n")
    report["source_notes_sha256"] = hashlib.sha256(a.read_bytes()).hexdigest()
    report["open_questions_sha256"] = hashlib.sha256(b.read_bytes()).hexdigest()
    output = Path(args.out)
    output.parent.mkdir(exist_ok=True, parents=True)
    output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n",
                      encoding="utf-8")
    for mode, data in report["methods"].items():
        print(f"{mode}: recall@3={data['recall_at_3']:.3f} "
              f"mrr@3={data['mrr_at_3']:.3f} "
              f"ood_abstention={data['ood_abstention_rate']:.3f}")
    print("polymath_v2_retrieval=PASS domain_comprehension=NOT_ASSESSED")


if __name__ == "__main__":
    main()
