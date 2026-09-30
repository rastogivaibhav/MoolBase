# EP-PROCESS-V2 — Cycle 2 Measurement Validity Audit

Status: interpretation audit of immutable V1 evidence. V1 is not rescored.

## Core conclusion

V1 preserved the emitted behavior correctly, but not every layer delta is a like-for-like causal comparison.

G0 is the clearest example. Its frozen capability contract deliberately builds persistent Graphene evidence/provenance state **without hypothesis selection**. Therefore G0's 0% terminal coverage is a real frozen output result, but it is not valid evidence that Graphene failed to retain evidence or that G0 is a 0%-accurate answer-producing engine.

## Capability contracts

| Config | Persistent evidence/provenance | Hypothesis selection | DWM | V1 output |
|---|---|---|---|---|
| B0 | no | current-observation rule | no | provisional support or abstain |
| G0 | yes | **no** | no | open, no hypothesis, cumulative evidence refs |
| G1 | yes | HypoKosh | no | selected/provisional or open hypothesis |
| G2 | yes | HypoKosh | yes | same hypotheses as G1 in V1 plus challenge/reopen and more contested/open states |

## G0 finding

The frozen G0 path:
- executes Graphene evidence expansion;
- builds an authoritative FiberBundle;
- preserves evidence edges/families;
- disables HypoKosh convergence and DWM;
- emits terminal cause `g0_evidence_projection_no_hypothesis_selection`.

The runner therefore emits `open` with no hypothesis at every step.

Yet G0 demonstrably retained history: at the final step of the four episodes it referenced all observations seen so far: **5/5, 5/5, 6/6, 4/4**, whereas B0 retained only the current event. G0 also had 100% receipt completeness under its own applicable-field contract.

Correct interpretation: **0% answer coverage is a capability-contract consequence with real no-answer output, not evidence of failed persistence/provenance.**

## G1→G2 finding

- Same terminal hypothesis in all four episodes.
- Same 75% terminal accuracy.
- G2: +19 challenges, +19 reopens, +0 revisions.
- G2: 19 total recovery expansions vs G1's 15.
- Final visited-state and selected-evidence counts matched G1 at every observation step.
- G2 frequently changed status from G1's provisional state to open/contested.
- V1 selective coverage still counted open + non-null hypothesis as covered, so this caution was largely invisible to coverage/accuracy.

EP04 is especially informative: expected ABSTAIN, but G2 produced open/contested H2. V1 counted this as covered-but-wrong rather than uncommitted.

## Metric applicability

| Metric | B0→G0 | G0→G1 | G1→G2 |
|---|---|---|---|
| evidence use | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_CAUSAL_COMPARISON` |
| refutation response | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` |
| revision inertia | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `INSUFFICIENT_EVIDENCE` |
| false convergence | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` |
| independent evidence convergence | `INSUFFICIENT_EVIDENCE` | `INSUFFICIENT_EVIDENCE` | `INSUFFICIENT_EVIDENCE` |
| selective coverage | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_DESCRIPTIVE_ONLY` |
| receipt completeness | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` |
| execution cost | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_CAUSAL_COMPARISON` | `VALID_CAUSAL_COMPARISON` |

## Why

- G0 has no operative hypothesis, so hypothesis-based evidence-use and terminal-answer metrics cannot test its intended capability.
- G0→G1 adds an answer interface as well as HypoKosh reasoning, confounding terminal deltas.
- G1/G2 share the answer interface, so evidence-use and runtime-cost deltas are more causal.
- V1 refutation response is temporally confounded because within-call transition events miss prompt cross-step changes.
- Revision inertia is not informative when no revision occurs; a value of zero can coexist with no response.
- False convergence is descriptively useful but weak here because no relevant configuration emitted a fully resolved state.
- Earned independent-evidence resolution is null because there were no resolved episodes.
- Receipt completeness uses different applicable field sets by capability.
- B0 and G0 timing paths are not comparable: B0 is local Python while G0+ includes compiled-runtime subprocess work.

## Corrected V1 interpretation

1. B0 is a stateless lower bound, not a robust reasoning benchmark.
2. G0 proves persistent evidence/provenance execution but does not provide hypothesis selection.
3. G1 adds hypothesis selection/governed convergence.
4. G2 adds native dialectical challenge/reopen and greater epistemic caution.
5. V1 does not establish that G1 or G2 improve terminal accuracy over B0, or that G2 produces earned belief revision.

## V2 implications

- Give Graphene-only persistence its own evidence/provenance endpoints.
- If a terminal-answer ablation is needed, create a separately named common-decision-head profile rather than redefining historical G0.
- Separate `operative_hypothesis` from `committed_answer`.
- Record cross-step belief changes explicitly.
- Evaluate DWM on challenge precision, reopen usefulness, contradiction sensitivity, status calibration, frontier/ranking change and justified revision.
- Preregister all V2 metrics and thresholds before score-bearing execution.

## Conclusion

The frozen V1 results remain unchanged. The defensible capability map is:

**G0 = persistence/provenance execution; G1 = hypothesis selection; G2 = dialectical challenge/reopen plus caution. V1 does not yet prove added terminal accuracy or earned revision.**
