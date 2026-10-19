# F15 — Twenty-topic associative-memory test

**Preregistered on 19 October 2026 BEFORE the topic corpus, runner, results or scorer are committed.** Base: `dacfb1697993a46e53898d70429da69ea1120936` (v14). All prior F1–F14 files and scores unchanged.

## User hypothesis
After reading about Hinduism, the Ramayana and Garuda, and then Southeast Asian culture, the knowledge store might associate India, Indonesia and Thailand. Test whether the **existing** native GrapheneDB and HypoKosh/DWM runtime can answer or abstain on the question "Where does the Garuda airline belong?" including a misspelling "gradua".

## Frozen inputs
Create exactly **20 substantial original-language authored knowledge articles**, spanning Indian Ramayana, characters, Garuda/Vishnu, Thai Ramakien and royal symbolism, Indonesian wayang/Prambanan/Bali/Garuda Pancasila, maritime transmission, Cambodia/Laos/Malaysia/Myanmar adaptations, and competing airline/country symbols. Do not treat summaries as externally attested documents; record their official/reference URLs and `project_authored_unverified_summary`. One airline article contains the **direct, officially sourced country assertion** that Garuda Indonesia is Indonesia's national flag carrier; a separate article explains airline/cultural symbolism without explicitly giving the country of the Garuda Indonesia company. All other cultural articles are incomplete evidence for airline corporate nationality.

Two corpora derived from the **same 20 frozen articles**:
- **C0 (knowledge-gap)**: withhold exactly the direct airline-identity article (19 docs). Keep all country and Garuda cultural distractions.
- **C1 (evidence-complete)**: all 20 articles, including direct official airline assertion.
No gold answer, country labels, manually inserted cause/association edges or manually preselected country/hypothesis nodes passed into the native reasoning graph. Metadata includes `topic_id`, title, article text and source URL only.

## Queries
Q1 = "Which country does the Garuda airline belong to?".
Q2 = "Where is possible gradua airline belongs to?" (original user misspelling).
Q3 = "In which country is Garuda Pancasila a national emblem?" (Indonesia cultural disambiguation).
Q4 = "Does a Garuda royal symbol in Thailand establish that Garuda Indonesia is a Thai airline?" (test distinction between symbols and companies).

## Arms / outputs
Run all 4 questions × C0/C1 = 8 controlled cases with:
- a lightweight non-neural lexical retrieval baseline;
- real `GrapheneDB::vector_search` top 5, metadata, score and deterministic text vectors;
- actual `CompleteHypoKoshRuntime::reason` G0/G1/G2 on the same text-node graph under Theoretical mode, update_model_world=false (24 actual native calls).

**IMPORTANT:** Native runtime returns selected **node IDs and epistemic status**, not arbitrary prose answers or country extraction. Do not hallucinate an answer interface: report retrieval of relevant article, whether any selected node has a supporting direct answer, abstentions and DWM challenges. A separate evaluator may detect an exact *pre-frozen textual fact string* in retrieved articles to measure evidence coverage, but must NOT call this a model-generated correct answer. Do not feed scorer labels or direct manual relation edges into the runtime.

## Acceptance/negative controls
- All 20 distinct substantive documents, with 19-vs-20 corpus integrity, source labels clearly unverified.
- Document and query token hashes, exact source rev, raw native receipt, archived logs and SHA-256 manifest.
- No direct airline nationality passage in C0. Only in C1 could airline country be answered from direct evidence; otherwise the correct epistemic outcome is unresolved. A cultural crosslink, keyword collision or Thailand royal emblem alone is not proof of corporate nationality.
- Native data persist/reopen. All query inputs/corpus docs remain identical across arms; do not modify after seeing results. Explicitly report if the lexical baseline beats graph vector retrieval, if semantic matching fails, or if DWM cannot produce an answer.
- Independent scientific generalization, authenticated third-party source documents, trained long-term learning and autonomous external research = NOT_RUN.
