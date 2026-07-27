# HypoKosh Answerable-Reasoning Hardening and Test Report

## Scope

This iteration fixes the weaknesses observed after the initial three-case HotpotQA pilot without changing GrapheneDB durable storage semantics.

## Fixes implemented

1. Replaced global yes/no negation detection with query-relevant, source-scoped evidence handling. An unrelated sentence containing “not” can no longer flip the answer.
2. Added governed answer operators for dates, numeric answers, and locations.
3. Added minimum-evidence de-duplication and explicit one- or two-atom evidence bounds for deterministic operators.
4. Tightened `RESOLVED` promotion: governed answers require query compatibility, sufficient evidence, confidence of at least 0.75, no detected contradictory evidence, and no silent promotion.
5. Preserved the existing domain-specific epistemic resolver when no governed answer operator applies. This fixed a regression where valid non-QA workflows could be downgraded to `EVIDENCE_REQUIRED`.

## Validation

- Full CMake build completed.
- Full CTest suite: **45/45 passed** in 14.88 seconds.
- Original HotpotQA oracle pilot: **3/3 answers correct**, all `RESOLVED`, each with two evidence atoms, and no silent promotion.
- New operator regression cases: date, numeric, and location cases resolved correctly with one minimal evidence atom.
- Contradictory yes/no case returned `no` but deliberately remained `EVIDENCE_REQUIRED`, rather than incorrectly claiming `RESOLVED`.

## Current boundary

The new operators remain deterministic extraction rules. They are safer and broader than the initial implementation, but they do not yet constitute a general open-domain answer generator. The next scientific gate should be a frozen unseen corpus rather than adding more handcrafted operators indefinitely.
