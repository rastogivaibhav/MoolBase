# Polymath Foundation v2 — Prespecified Real-Source Intake and Evidence Retrieval

Experiment: `MOOL-POLYMATH-F2-001` — frozen at commit creation on 2026-10-18 **before v2 retrieval code or scored question list**.

## Research objective and epistemic boundary

Test whether a **small, source-grounded knowledge intake** can preserve source identity, licensing posture, citation anchors, ontology role boundaries and known unknowns; and whether a fixed evidence-retrieval policy can locate relevant facts without inventing evidence. **This is not an LLM comprehension, cross-domain invention, offline incubation or causal discovery benchmark.**

The preceding v1 fixture was explicitly artificial. V2 must reference externally published sources and keep individually labelled, newly authored explanatory summaries separate from original publisher text. We will NOT import publisher text into a trainable LLM; avoid OpenStax material that explicitly restricts AI use. The initial corpus will comprise at least six citation-grounded source notes across:
- networks: TCP congestion control and ECN, RFC Editor;
- biology: homeostatic negative feedback and its limits, peer-reviewed physiology articles indexed by PubMed Central;
- supply chain: inventory reorder-point and safety-stock definitions, Oracle/Microsoft technical product documentation.

All records must label `content_form=author_paraphrase`, `source_kind=external_public_reference`, `verification_status=human_review_needed`, and `allow_model_training=false`. A URL being reachable is NOT source authentication; a local SHA-256 digest proves only that the curated note is unchanged. Never relabel a derived note as the original publisher's verbatim text or as `Verified` evidence.

## V2 source contract

Each source record requires: globally unique source ID, canonical https URL, title, publisher, domain, publication or access date, source family, document version/section anchor, license status marked `not_relicensed` or `review_required`, and declared usage `citation_and_author_paraphrase_only`. Each note must bind to exactly one source ID and section/anchor, possess a domain, a stable concept ID, an original author-written note (not asserted to be exact source text), typed roles, assumptions and explicit applicability limits. Each note is a **curation assertion** with an externally inspectable pointer, not a new independent observation. Source-family deduplication must be enforced.

## Fixed retrieval methods to compare

**B0 — lexical baseline:** simple lowercase alphanumeric unigram TF-IDF cosine retrieval over author's note text and short concept label, with no trained embeddings and no domain/type boosting.

**B1 — structured metadata:** same TF-IDF cosine score over the same note text and label, plus:
- `+0.15` if the question declares a domain matching the note;
- `+0.10` for each exact controlled `required_role` present, capped at `+0.20`;
- filter source families to at most one note per family per result;
- if a question is out of corpus, it must abstain rather than fabricating an answer.

All constants fixed before evaluation. B1 does not transform terms through an LLM; the question's domain/role tags will be manually authored, which is an **information advantage**, not free intelligence. B1 may therefore help or hurt relative to B0. Both use same notes, no future data. Evaluate recall@3, MRR and source-family diversity on at least 12 prewritten domain questions. No posthoc retuning. Never score response semantic accuracy because no answer generation is implemented.

Out-of-corpus questions must be marked explicitly in the fixture, scored as a control for abstention. An in-corpus question with zero shared query tokens may return a candidate with the `insufficient_lexical_evidence` label, but does not count as answered.

## Acceptance gates

**G0 Provenance and license integrity:** no record accepted without source ID, section anchor, declared rights posture, source family, nonempty explicit boundary or clear synthetic/derived status. Tests must reject forged `verified`, `verbatim`, unsupported license, duplicated source identifiers and dangling source references. No ingestion into `EdgeOrigin::Observed` or `ModelWorldStatus::Verified`.

**G1 Retrieval mechanics:** reproducible ranked evidence trace, valid source URLs/section anchors, duplicate-family suppression, nonmatching-domain guard, safe out-of-corpus abstention and immutable input hash.

**G2 Research utility (descriptive only):** report per-domain and pooled B0 vs B1 recall@3 and MRR; no preregistered superiority claim or significance on tiny open question fixture. Report failures and missing evidence explicitly.

**G3 Comprehension: NOT ASSESSED:** A genuine test would require independently authored blind tasks, a model permitted to read the material, compositional transfer questions and independently scored answers. That belongs to a separate preregistered phase; B1 retrieval scores cannot be called subject mastery.

## Negative controls

Reversed causation and `depends_on` versus `uses`; duplicated source families; lexical-similar cash flow vs network flow; out-of-scope questions, source-license mismatch, notes without external anchors, and fabricated source verification must be rejected/flagged. Preserve any null and negative results.

## Engineering scope

A Python stdlib-only ingestion/validation + retrieval benchmark under `research/polymath_foundation/v2/`; no network fetch at runtime, no LLM or autonomous tool, no mutation of production MoolBase evidence stores. All source notes committed with SHA-256 evidence receipt, GitHub Actions runs tests, and attaches outputs. No change to frozen earlier epistemic experimental contracts or v1 fixture. PR stacked on v1 #121 until ontology review completes.
