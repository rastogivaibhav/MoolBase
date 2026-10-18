# MoolBase Polymath Foundation v3 — Applied Domain Comprehension Instrument

**Status:** Research instrument, not a demonstrated intelligent learner. Stacked on v2 PR #122, v1 PR #121. Frozen experiment \`MOOL-POLYMATH-F3-001\` is in \`PREREGISTRATION.md\`; created before instrument code.

## What is implemented

V1 tested ontology-direction safety. V2 tested evidence-source metadata and citation-aware retrieval on attributed *author paraphrases*, discovering a real recall/rank trade-off. V3 targets a different question: **can future agents apply within-domain concepts to novel parameterised scenarios and abstain when assumptions fail?**

Three independently scoped, *project-authored* pedagogical cards:
- Networks: a **simplified finite queue**, not actual TCP algorithm semantics.
- Biology: a **simplified saturated proportional regulator**, not a validated physiology model.
- Supply chain: a **simplified inventory reorder policy**, not an Oracle/Microsoft ERP guarantee.

The cards do not reproduce publisher text. Their external reference IDs point to v2 notes **for context only** and do not confer any publisher's licensing or authority. External training rights remain unproven; repository publication rights for the authored cards should be reviewed before wide redistribution.

A deterministic generator produces **72 application tasks**, including **60 valid quantitative cases** and **12 invalid-assumption/boundary cases** across three domains. It exports question identity, variable values and the allowed learning-card ID — **not gold answers**. The public generator is repeatable, so this is *not* an independently blind or contamination-free assessment.

A separate scorer evaluates submitted predictions against a deterministic private-in-process arithmetic oracle (no real-world ground truth). It requires card citation and assumption acknowledgement. It reports per-domain accuracy, boundary abstention, unsupported numerical assertions, citation validity and coverage. It rejects tampered challenge hashes, duplicate IDs and malformed/nonfinite responses.

## Actual executed baselines and their interpretation

- \`reference_oracle_not_learned\`: implements the known equations; expected **72/72**. Tests generator and grader correctness only, **no understanding**.
- \`abstain_only_not_learned\`: never computes; expected **12/72** (passes boundary cases only). Tests the grader does not reward nonanswers on application tasks.
- \`external_learned_candidate\`: **NOT RUN**. Neither MoolBase nor an LLM was connected to a tutoring/answering agent in this v3 milestone.

No model's competence is established by the reference solver. No autonomous research or cross-domain hypothesis creation is attempted. The word *independent* here describes separation of prediction submission from oracle scoring, **not** independent benchmark authorship or independent scientific outcome adjudication.

## Reproduction

From repository root:

\`\`\`bash
python3 research/polymath_foundation/v3/test_instrument.py

python3 research/polymath_foundation/v3/challenge.py \
  --cards research/polymath_foundation/v3/cards.json \
  --out reports/polymath_foundation_v3/challenges.json

python3 research/polymath_foundation/v3/scorer.py \
  --cards research/polymath_foundation/v3/cards.json \
  --challenge reports/polymath_foundation_v3/challenges.json \
  --selftest reference_oracle_not_learned \
  --out reports/polymath_foundation_v3/oracle_selftest.json

python3 research/polymath_foundation/v3/scorer.py \
  --cards research/polymath_foundation/v3/cards.json \
  --challenge reports/polymath_foundation_v3/challenges.json \
  --selftest abstain_only_not_learned \
  --out reports/polymath_foundation_v3/abstain_selftest.json
\`\`\`

To test a real external model later, give it only \`cards.json\` and \`challenges.json\` (not the scorer or reference output). Record its actual predictions separately as a structured submission. Grade with \`--submission path/to/actual_responses.json\`. The reported \`model_executed\` flag is a declaration, not cryptographic proof of inference; future independent runners must authenticate it. The output receipt contains no gold answers.

Example response format:

\`\`\`json
{
  "experiment": "MOOL-POLYMATH-F3-001",
  "participant": "candidate-name",
  "challenge_sha256": "SHA256_OF_CANONICAL_CHALLENGE",
  "model_executed": true,
  "responses": [
    {
      "task_id": "f3-1-01",
      "evidence_card_id": "card-network-queue-v1",
      "assumptions_acknowledged": true,
      "confidence": 0.72,
      "answer_type": "numeric",
      "value": 11
    }
  ]
}
\`\`\`

All 72 answers are required. The example is structural only, not the correct answer to any benchmark item.

## Gates before claims about intelligence

1. **Rights review:** Validate provenance and explicit rights for any authentic document ingested as training material. The v2 external-source notes were restricted to citation and paraphrase.
2. **Independent assessment:** Experts must author entirely new, held-back application and counterexample questions, ideally about real physical/scientific systems with independently verified outcomes.
3. **Actual learner:** Connect a concrete baseline LLM/RAG and a MoolBase domain learner through the same inference budget, identical reading texts and immutable prediction receipts.
4. **Measured domain skill:** Assess unseen task correctness, citation fidelity, abstention, calibration, and problem-solving transfer, distinguishing within-domain application from retrieving literal sentences.
5. **Cross-domain research:** Only when within-domain performance is demonstrated, preregister structural analogy generation, self-selected research questions, HypoKosh/DWM falsification and offline incubation with blind test oracles.

## Limitations / merge policy

The v3 instrument's task generator and oracle are public and encode the same simple rules, making benchmark contamination straightforward. Do not report reference solver accuracy as an AI score. v3 is a **scoring-readiness milestone**, not a scientific result demonstrating MoolBase's understanding.

Keep v3 unmerged until v1/v2 stacked PR review, run exact-head CI and evaluate rights/ontology compatibility. No frozen P0-P4, Epistemic Process V1-V3 or prior Foundation V1/V2 score-bearing protocols are modified.
