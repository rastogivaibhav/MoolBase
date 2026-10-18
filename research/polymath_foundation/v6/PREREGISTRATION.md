# MOOL-POLYMATH-F6-001 — Preregistered typed mechanism / validated execution pilot

**Protocol committed 2026-10-18 before F6 code, questions or outcome inspection.** Base is the exact successful native Foundation v5 commit `f40822213e989d48fd3d4216a61cd4efb23e7b9b`. All F1–F5 and EP-PROCESS frozen protocols/scores remain untouched.

## Question / hypotheses

Foundation v5 executed the real database and HypoKosh–DWM, but all four 1.5B-model arms solved **0/11 valid arithmetic applications**; native DWM correctly required evidence. We now test engineering feasibility: **can a database-backed, typed and explicitly authorized mechanism interpreter** consistently enforce input units, invariants and deterministic arithmetic on unseen parameters, without asking an LLM to perform calculations?

**This is mechanism executor validation, NOT polymath generalization**. The three mechanisms are pre-authored, public and toy. The next scientific gate (external blinded mechanism choice, independently sourced observations and model-selected procedures) is NOT part of this test. A correct execution of provided equations does not demonstrate an intelligent model inferred them, a theory is true in nature, or MoolBase has acquired scientific understanding.

## Mechanism representation and production boundaries

Provide three canonical, reviewed, versioned procedure IDs, one per domain, derived solely from project-authored Foundation v3 instructional cards:
- `toy.network.finite_queue.v1` — packets/counts are nonnegative integers, queue <= buffer. `unserved = max(0,queue+arrivals-service)`; `next_queue = min(buffer,unserved)`; `drops = max(0,unserved-buffer)`.
- `toy.bio.saturated_regulator.v1` — finite state scalars, dimensionless nonnegative gain, strictly positive control limit. `control = clip(gain*(setpoint-observed),-limit,+limit)`; `next_state=observed+disturbance+control`.
- `toy.supply.reorder.v1` — integer inventory units, demand units/day, lead_days days; all quantities nonnegative; `position=on_hand+on_order-backorders`; `reorder_point=daily_demand*lead_days+safety`; `order_qty=max(0,target-position)` iff `position<=reorder_point`, else 0.

All procedures are compiled into a constrained C++ allowlist. **Do not eval expressions from source text, generate arbitrary code, or silently repair invalid input.** Dataset manifest binds procedure ID, exact domain, card, version, author, source family, `evidence_status=unverified`, typed input/target schemas and boundary contract. Mechanisms are stored as MoolBase `NodeInput` directly (never via generic relation ingestor, which labels caller claims Observed). Separate `GrapheneDB::metadata_search` retrieves exactly one unverified procedure by ID and rejects missing/duplicate/wrong-domain/rights state; no Verified/Observed edges or facts are written.

Every actual execution receipt must include: mechanism ID/version, input/target units and type checks, original values, deterministic outputs OR named abstention reason, native source node ID, source family, policy version, and `scientific_truth_claim=false`. Guard all integer arithmetic against overflow. A target must be an allowable output of the registered procedure; a declared input unit must exactly match its specification. A query without units is rejected, not guessed.

## Test design (fixed prior to implementation)

Use the **original v3 challenge generator** with alternate fixed seed **20261019**, producing 72 cases, 24/domain: 60 valid arithmetic problems + 12 invalid-assumption boundary cases (20+4/domain), covering each domain's two outputs. This is a second public deterministic corpus, not independently authored blind questions. All questions are generated prior to scoring and input hashes recorded.

Python study adapter adds typed unit tags defined in the immutable native manifest. It must **not** expose gold answers, computed features or v3 oracle to the native runner. Independent Python v3 oracle grades the native C++ output; exact numeric tolerance 1e-9, with 64-bit integer overflow input cases excluded from the 72 but separately tested as hard stops. Unimplemented model/task-to-procedure selection is marked NOT_RUN.

Prespecified negative controls:
1. Missing fields; missing/incorrect unit; wrong scalar vs integer; negative/NaN/Infinity.
2. Domain mismatch; wrong target; unknown procedure ID; source status falsely set Verified; duplicate mechanism ID/ontology mapping; modified source family; unknown source.
3. Network invalid buffer/arrivals, biology negative gain/nonpositive limit, supply unknown lead time/negative demand.
4. Integer overflow and biologic floating-point overflow; stale card/content hash or altered mechanism version; reopened database round trip.
5. Abstention must NOT quietly emit a numeric value. Correctness must require evidence citation and explicit status `mechanism_executed_unverified`, not natural-language assertions.

## Baselines and report

- **B0 F5 observed result (historical context only):** all four model arms 1/12 total, 0/11 valid on a DIFFERENT sample and different model. **Not directly comparable** to F6.
- **B1 Native typed interpreter:** execute all 72 cases through the real C++ MoolBase storage/retrieval and compiled procedure registry. Expected mathematically exact *by construction*, but measure actual.
- **B2 Ordinary Python oracle:** independent implementation from original v3 benchmark used only to score; never used as live interpreter or answer source.
- **B3 Model-only / model-selected mechanism:** NOT_RUN. No false AI intelligence or external independent question claims.

Report 72-case accuracy, valid application, boundary abstention, unit/provenance negative controls, parsing errors, native DB restarts and input SHA. No claim of statistical superiority vs F5, which uses a different sample. No tuning based on observed scores without new protocol.

## Research conditions after F6
- Independently authored blind tasks (withhold exact procedure identity, vary units and inject distractors).
- Equal tool availability and computation for models selecting mechanisms, a strong external LLM baseline, RAG and MoolBase+HypoKosh–DWM.
- Externally verified physical/biological/logistics evidence, reproducible observations and source licenses.
- Rejected or challenged procedures must not be promoted to Verified; independent scientific evidence gates remain mandatory.

The acceptance condition is **implementation safety, determinism and clear negative controls**, not evidence of polymath learning.
