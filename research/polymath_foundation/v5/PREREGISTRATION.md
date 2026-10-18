# F5 Native MoolBase Evidence Retrieval + Governed HypoKosh–DWM Pilot

**Experiment MOOL-POLYMATH-F5-001. Preregistered 2026-10-18 before F5 implementation, model execution or F5 scores.** All prior results remain frozen; stack on #124.

## Objective and boundaries

Evaluate whether **actual C++ MoolBase evidence retrieval** supplies context to a larger, real inference model, and whether **actual HypoKosh–DWM runtime traces** can be included as bounded, epistemically honest hints. The study examines integrations, not independently blinded science, autonomous domain learning or established polymath intelligence.

The current `GenericRelationIngestor` stamps caller claims `EdgeOrigin::Observed` and does not authenticate them. F5 **must not use it** for ingesting toy pedagogical cards. Add project-authored toy concept nodes directly into GrapheneDB with source metadata `project_authored_toy`, `verification=unverified`, `training_rights=not_external_publisher_text`. Any relation is `EdgeOrigin::Hypothetical`. No production `ModelWorldStatus::Verified` promotion or fabricated evidence refs.

## Frozen scope, sample and LLM

Use original F3 seed **20261018**, but take **tasks at domain-local positions 9–12 (indices 8..11)**, different from F4's first eight. Exactly **12 tasks**, four per domain; report valid and invalid-assumption counts from oracle. This is a *public deterministic generator*, NOT independently authored/secret holdout. The result is descriptive.

Same larger model on all arms: official `Qwen/Qwen2.5-1.5B-Instruct-GGUF`, `qwen2.5-1.5b-instruct-q4_k_m.gguf`, model repo SHA `91cad51170dc346986eccefdc2dd33a9da36ead9`; Apache-2.0. Download once and hash model bytes. Pinned llama.cpp commit `ff5a23376725e5a889fa0bfb987b7541dc304da0` CPU-only. Temperature 0, seed 20261018, context 4096, max new tokens **160**, one request per task, stateless, no test answer in prompt. Do not tune to scores.

Four arms on identical 12 tasks in predeclared order:

- **A0 No card:** task only, same answer schema.
- **A1 Direct card:** project-authored card from F3, exactly as published.
- **A2 Native MoolBase retrieval:** same card text but returned by **actual GrapheneDB** metadata search and `get_node()` after source metadata validation; no Python dictionary pretending to be a database.
- **A3 Native retrieval + actual HypoKosh–DWM:** same native-retrieved card and *real* `CompleteHypoKoshRuntime::reason()` trace with `enable_hypokosh=true`, `enable_dwm=true`, `update_model_world=false`, and speculative-only message. No independent evidence means `Resolved/Verified` must not be claimed; if native engine returns no meaningful discriminating tests, report that rather than fabricating one.

Total calls **48**; fixed arm order A0→A1→A2→A3. A1/A2 prompt text should be byte-for-byte equivalent except a provenance metadata banner added separately to all knowledge arms, to isolate storage/retrieval. If byte-equivalence cannot be maintained, report prompt difference. In A3, extra runtime trace incurs greater input tokens; output budget equal but **not compute-matched**, and differences cannot establish incremental reasoning efficacy.

## Native adapter requirements and safety tests

Native C++ CLI built from real `GrapheneDB`:
1. Load the 3 project-authored cards from a tab-separated validated manifest (generated from canonical cards.json, not independent scientific sources).
2. `put_node` original project content, metadata `domain`, `card_id`, `source_family`, `evidence_status=unverified`, `origin=project_authored_pedagogical`, deterministic hash-vector/signature. Store separate evidence/label node if needed but never `Observed` or `Verified`.
3. Query by declared task domain using **GrapheneDB::metadata_search** and `get_node`, and fail closed unless exactly one matching card with the matching card ID, source type and nonempty assumptions and boundaries.
4. Run actual `CompleteHypoKoshRuntime::reason` for A3 and capture status, hypotheses/challenges, audit events. It must not insert verified facts or treat the toy diagram as authenticated empirical evidence; persist trace, mark as nonauthoritative and keep `update_model_world=false`.
5. Negative tests: missing domain, duplicate card, unknown card, altered source state, retrieval mismatch, bogus `Verified`, source-family laundering, and improper native engine bypass. Test restart and unchanged retrieval where feasible.

## Scoring

Reuse F3 numerical/boundary oracle unchanged. Score exactly the 12 frozen tasks per arm and record per-domain, boundary abstention, malformed JSON, numeric accuracy, citation correctness, unsupported numeric assertions, question/corpus hash, exact prompt, raw response, model and engine SHA-256, status/trace and token counts. Parse errors score zero, including boundary cases.

No superiority or significance claim on 12 unblinded tasks. If native/DWM integration fails, mark corresponding arm **NOT_RUN** and never use A1 answers as replacements. The scripted v3 oracle and v4 small-model results are not counted as arms.

## Acceptance

F5-G0: freeze protocol before coding/scoring; preserve frozen v1–v4 research files and scores.
F5-G1: native C++ manifest ingestion + real GrapheneDB retrieval pass integrity and negative tests.
F5-G2: actual `CompleteHypoKoshRuntime` invoked with HypoKosh/DWM enabled, true trace and strict speculative/no verified posture; otherwise A3 NOT_RUN.
F5-G3: real 1.5B model completed all calls with preserved raw model output and fail-closed scoring.
F5-G4: all four arm results reported including null/negative; no misleading competence or causal benefit claim.

**Following this pilot:** separately commission rights-cleared independent unseen questions, source-authenticated evidence and cost-matched evaluation. This study alone cannot establish autonomous knowledge acquisition, scientific discoveries, causal transfer, or polymath generalisation.
