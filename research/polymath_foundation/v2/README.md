# MoolBase Polymath Foundation v2 — Source-Grounded Intake and Comprehension Gate

**Status:** Research prototype, 18 October 2026; stacked on Foundation v1 PR #121. **No claim of demonstrated subject comprehension, cross-domain invention, literature self-study, verified evidence or autonomous research.**

## Why this is different from v1

Foundation v1 proved the direction-aware relation ontology and the schema/triage rules on **synthetic** concepts. V2 brings in attributed references to real external publications from three disciplines and tests whether a bounded retrieval policy can find suitable evidence without laundering source credibility.

This does **not** download full publisher texts or ingest them into model weights. The local collection holds short original author-written paraphrases with external URLs and section anchors. They are not assumed independently authenticated. We deliberately did not ingest OpenStax material because reuse conditions surfaced specific restrictions for AI applications.

## Sources and rights

See `source_notes.json`. Seven named source records: RFC 5681 and RFC 3168 (networks); two PMC-hosted physiology research works (biology); Oracle Inventory reorder-point planning, Microsoft safety-stock and Business Central reorder planning docs (supply chain). They are traceable and permit human review, but the prototype **does not establish permission for ingesting a publisher's full text** or for model training. Publisher terms and licence must be checked before deployment.

V2 keeps `verification_status=human_review_needed`, `content_form=author_paraphrase`, `allow_model_training=false`, and a declared rights posture. Notes are curation assertions, **never raw authoritative quotations or `Observed/Verified` facts**. A local SHA digest attests only to file immutability, not source authenticity, external truth, independence or copyright clearance.

## Research question and measured mechanism

Experiment `MOOL-POLYMATH-F2-001`, preregistered before the source collection and scorer. B0 = lexical TF-IDF over short labelled note texts. B1 = same lexical score plus a fixed +0.15 match for declared domain and +0.10 per controlled mechanism role (up to +0.20); filters duplicate source families and will not fabricate relevance from metadata if there is zero lexical overlap.

The questions in `evaluation_questions.json` were authored openly, are NOT independent blind assessments, and have known target source notes. Retrieval measures **recall@3**, **MRR@3** and out-of-corpus abstention, not answer correctness or understanding. If B1 appears better than B0, it shows benefits of author-provided domain/role hints on a tiny open fixture; it does not prove the engine found concepts by itself.

### Reproduce

```bash
python3 research/polymath_foundation/v2/test_intake_retrieval.py
python3 research/polymath_foundation/v2/intake_retrieval.py \
  --sources research/polymath_foundation/v2/source_notes.json \
  --questions research/polymath_foundation/v2/evaluation_questions.json \
  --out reports/polymath_foundation_v2/retrieval.json
```

## Non-negotiable boundaries

- No source-note import into MoolBase production `Observed` or `Verified` status; generic relation import does not authenticate external sources, and remains blocked for unattended scientific evidence.
- No full document download or pretrained-model fine tuning from these sources.
- The relative URLs and section anchors are inspection pointers. URL reachability can change; the CI is deliberately reproducible offline.
- V2 ingestion is a validated reference registry and paraphrase-note ingestion, **not autonomous raw-document extraction**. That is a separate research and rights-clearance milestone.
- No model generates new domain concepts, questions, hypotheses or solutions.
- Out-of-corpus controls detect only zero lexical overlap, not full open-world unknowns.
- Fixed open questions test retrieval fidelity only; no assessment of knowledge retention or cross-domain generalization.

## Required v3 experimental gates

1. Copyright/license gate: obtain appropriate permission and use authentic, versioned source snapshots, verify document hashes and identity.
2. Independent blind domain QA: expert-written previously withheld application and counterexample questions; no answers in the read corpus; apply the same retrieval and compute budget to standard RAG and ontology representations. Report calibration, unsupported assertions and abstention.
3. Source-independent corroboration and counterexamples: evaluate directionality, ontology version/units, source family dependence, chronological validity and misleading analogies.
4. Cross-domain transfer: withhold one domain and its questions, train abstractions on others, then test transfer with source-verified outcomes.
5. Autonomous research agenda: compare question selection with random, similarity-only and strong compute-matched agents; never conflate more generated questions with useful discoveries.
6. Background incubation: preregister unseen future problem set and offline compute budget; require unprompted hypotheses to predate targets.

Do not claim `polymath` or `domain comprehension` success from v2's retrieval scores.
