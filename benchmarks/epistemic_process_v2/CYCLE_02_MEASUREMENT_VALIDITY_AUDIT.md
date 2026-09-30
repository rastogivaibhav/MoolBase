# EP-PROCESS-V2 — Cycle 2 Measurement Validity Audit

Status: interpretation audit of immutable V1 evidence. No V1 result is changed or rescored.

## Core conclusion

V1 correctly preserved what each configuration emitted, but several cross-layer metric deltas are not like-for-like capability comparisons.

The largest example is G0. Its 0% coverage is a direct consequence of a frozen contract that deliberately builds persistent Graphene evidence/provenance state **without hypothesis selection**. Calling G0 a 0%-accurate reasoning engine would therefore be incorrect.

The 0% is not erased: under the V1 terminal-answer evaluator, G0 truly supplied no answer on all four episodes. The correct interpretation is:

> Terminal-answer accuracy is non-applicable as a causal measure of G0's intended evidence/provenance contribution, while its no-answer output is a real consequence of the selected G0 capability boundary.

## Capability contracts actually exercised

| Config | Persistent Graphene evidence/provenance | Hypothesis selection | DWM challenge/reopen | Frozen V1 output |
|---|---|---|---|---|
| B0 | no | local current-observation rule only | no | provisional current support or abstain |
| G0 | yes | **no** | no | open, no hypothesis; cumulative evidence refs |
| G1 | yes | yes, HypoKosh | no | selected/provisional or open hypothesis |
| G2 | yes | yes, HypoKosh | yes | same selected hypotheses as G1 in V1, with challenge/reopen and more contested/open states |

## G0: what actually happened

- Production explicitly disables HypoKosh for G0.
- G0 constructs a Graphene evidence expansion and FiberBundle.
- It marks Graphene executed and preserves evidence edges/families.
- It intentionally does not run stability/admissibility/convergence/opposition.
- Its terminal cause is `g0_evidence_projection_no_hypothesis_selection`.
- The benchmark runner projects each G0 step as `status=open`, `hypothesis=null`.
- At each final episode step, G0 referenced all observations seen so far: 5/5, 5/5, 6/6 and 4/4. B0 referenced only the current observation.
- G0 receipt completeness was 100% under its applicable-field contract.
- V1 recorded 61 evidence-edge references and 162 visited states for G0 across the four episodes.

Therefore, `G0 coverage = 0%` is factually correct but is an **interface/capability-contract outcome**, not evidence that Graphene failed to retain evidence.

## G1→G2: what DWM actually changed

- G1 and G2 selected the same terminal hypothesis in all four episodes; terminal accuracy remained 75%.
- G2 emitted 19 challenge events and 19 reopen transitions; G1 emitted none.
- G2 performed 19 total recovery expansions vs G1's 15: four additional expansions.
- Final visited-state counts and selected evidence-edge counts matched G1 at every observation step.
- G2 frequently downgraded the epistemic state from G1's provisional state to open/contested where alternatives or opposition existed.
- The V1 selective-coverage evaluator defines coverage as non-abstain + non-null hypothesis. It therefore still counts an `open` receipt carrying a hypothesis as covered.
- In EP04, where the expected terminal result was ABSTAIN, G2 emitted open/contested H2. V1 counted that as covered-but-wrong rather than uncommitted.

V1 therefore supports a **process claim** that DWM adds challenge/reopen behavior and additional caution. It does not support a terminal-accuracy or belief-revision benefit claim.

## Metric applicability matrix

| Metric | B0→G0 | G0→G1 | G1→G2 | Reason |
|---|---|---|---|---|
| evidence use | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_CAUSAL_COMPARISON` | G0 has no operative hypothesis, so its evidence-use denominator is zero; G1/G2 share a decision interface and both score 100%. |
| refutation response | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | B0 has no persistent hypothesis and G0 has no selection. For G1/G2, V1 recognizes within-call reopen/revision but misses prompt cross-step hypothesis changes. |
| revision inertia | `NON_APPLICABLE_BY_CAPABILITY_CONTRACT` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `INSUFFICIENT_EVIDENCE` | Zero inertia can occur with no response transition; no V1 configuration emitted a revision. |
| false convergence | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | Rates are 0 largely because no relevant path emitted a resolved decision; useful safety observation, weak causal discriminator. |
| independent evidence convergence | `INSUFFICIENT_EVIDENCE` | `INSUFFICIENT_EVIDENCE` | `INSUFFICIENT_EVIDENCE` | Earned-resolution rate is null because there are no resolved episodes. |
| selective coverage | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_DESCRIPTIVE_ONLY` | G0 is structurally answerless. For G1/G2, open with a non-null hypothesis is still counted as covered, hiding G2's caution signal. |
| receipt completeness | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | `VALID_DESCRIPTIVE_ONLY` | Required-field sets differ by configuration/capability, so cross-config percentage deltas are not like-for-like quality scores. |
| execution cost | `CONFOUNDED_BY_INTERFACE_DIFFERENCE` | `VALID_CAUSAL_COMPARISON` | `VALID_CAUSAL_COMPARISON` | B0 timing is local Python while G0+ includes compiled-runtime subprocess work. G0/G1/G2 share the same runtime path; their layer cost deltas are more interpretable, although n=4 is tiny. |

## Corrected interpretation of V1 without changing V1

1. **B0** is a deterministic stateless lower bound that achieved 75% terminal accuracy by reacting only to the current observation. That is not evidence of robust epistemic reasoning.
2. **G0** demonstrably adds persistent evidence/provenance machinery, but V1 deliberately gives it no hypothesis-selection capability. Its 0% terminal score must not be presented as a failed version of G1.
3. **G1** adds hypothesis selection/governed convergence. Its return to 75% terminal accuracy shows the full Graphene+HypoKosh stack can produce answers, but G0→G1 terminal accuracy conflates adding an answer interface with subtler reasoning improvements.
4. **G2** adds native challenge/reopen behavior and more contested/open states. It does not improve terminal-hypothesis accuracy in V1 and emits no within-call revisions.
5. V1 is valuable precisely because the frozen experiment exposed these capability/measurement boundaries rather than allowing them to be optimized away.

## V2 design implications

- Retain a pure Graphene evidence-state configuration, but measure it with evidence-retention/provenance endpoints rather than terminal answer accuracy.
- If an answer-producing G0 ablation is needed, create a separately named V2 profile using a common decision head; do not silently redefine historical G0.
- Separate `operative_hypothesis` from `committed_answer`. A contested/open hypothesis should not automatically count as committed answer coverage.
- Add cross-step belief-change telemetry so H1→H2 or H1→ABSTAIN changes caused by new evidence are observable even when they occur before the within-call DWM loop.
- Measure DWM on its intended causal surfaces: challenge precision, reopen usefulness, contradiction sensitivity, status calibration, frontier/ranking change and justified revisions.
- Preregister all V2 metrics and effect thresholds before a score-bearing V2 run.

## Cycle 2 conclusion

The frozen V1 scores remain unchanged. The strongest capability-aware interpretation is:

**G0 proves persistence/provenance execution; G1 adds hypothesis selection; G2 adds dialectical challenge/reopen plus caution. V1 does not yet establish that those additions improve terminal accuracy or produce earned belief revision.**
