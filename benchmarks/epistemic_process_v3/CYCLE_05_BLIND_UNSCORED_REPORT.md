# EP-PROCESS-V3 — Cycle 5 Blind Unscored Mechanical Campaign

Status: **R2 EXECUTION GATE PASSED — TRANSITION COVERAGE INCOMPLETE**

## Objective

Execute the frozen 384-episode universe without oracle joins, correctness metrics, claim scores, or task-family labels. This report seals the existing R2 CI evidence; it does not claim that the later R3 production remediation passed.

## Verified Execution

Workflow run: https://github.com/rastogivaibhav/graphenedb_v1/actions/runs/36780063690

All dedicated workflow steps succeeded, including frozen-boundary protection, scoring-disabled checks, both campaign passes, both mechanical validations, deterministic comparison and artifact upload.

Each pass executed G0E, G1 and G2 across 384 episodes each: 1,152 executions per pass and 2,304 across two passes. Seed: `20261004`.

| Mechanical observation | Per pass |
| --- | ---: |
| Runtime failures | 0 |
| Validator-reported telemetry gaps | 0 |
| Step records | 3,792 |
| Recovery rounds | 2,610 |
| G2 challenge events | 288 |
| G2 corroboration-search events | 513 |
| G2 reopen events | 16 |
| G2 episodes with latent evidence references | 48 |
| G2 NEW_ELIGIBLE_NODE_AVAILABLE rounds | 16 |
| G2 NO_EXPANSION_OPPORTUNITY rounds | 1,297 |

G1 emitted no DWM challenge or reopen events. The two complete raw output files had identical SHA-256:
`1bf8c3c77e0ff07c50f09439f49d0195e1adc5f770119b9b5ed4a61262c69213`.

The 48 latent-reference episodes are an aggregate mechanical observation. They do not establish that 48 useful DWM reopens occurred or that DWM improved correctness.

## Blind and Frozen Boundary

The CI gate verified all nine frozen Cycle-2 contracts and the generator/task hashes. Production files and tests were protected relative to Cycle-4 latent-anchor remediation `65b12489764a0f4cd181549f53642d23d19264f5`.

Task candidate SHA-256:
`575c89930859bc6f23ecd218c7ed4c27032ec3a9f5a8c12bea1bbe84ab356829`.

Blind-input SHA-256:
`ed482e1da6c5230d53fac7dd1ab6ddd9296f92901e2e5060d6214e396ffc1bf2`.

Oracle joining, outcome metrics and score-bearing execution were false. This report uses only workflow logs and aggregate mechanical telemetry; it does not compare runtime answers with evaluator truth.

## Missing Mechanism Coverage

The native-event summary contained no revision, decommitment, recommitment or resolution events. It did contain `cross_step_belief_change` observations (296 for G1 and 296 for G2), which are not sufficient evidence of the frozen native transition events.

The current Cycle-5 validator enforces recovery-field presence and challenge/corroboration/reopen coverage. It does not require nonzero native revision/commitment/resolution event coverage. Therefore its zero reported telemetry gaps must not be presented as proof of complete transition coverage.

No resolved state appeared in the aggregate final-status summary. This is a coverage finding, not a correctness score or a diagnosis obtained by joining to the oracle.

Later native-transition remediation exists on PR #89 and an R3 campaign branch, but the inspected R3 head has failed general CI and no dedicated Cycle-5 run. R2 success must not be transferred to that different production head.

## Exact Repository and Artifact Evidence

- Campaign branch head: `6760332c6d38122ae7f266209697e4883b88512a`.
- Campaign branch tree: `dcfdf117ff362540728f5a903eeefc24af160636`.
- PR: https://github.com/rastogivaibhav/graphenedb_v1/pull/88
- Tested PR merge checkout: `52cfe10399b22bfbd9a716cf4a16111e9c662a66`.
- Dedicated workflow run: `36780063690`.
- Audit artifact ID: `11127094635`.
- Audit artifact digest: `sha256:88e625c0fab90174665d0667fb5fd1099ab209320c480b21daa7efa3ade52979`.
- Mechanical-summary SHA-256: `bcb853ac2d8871c6ca71233778adc3f2de5be3cfa73f0d027337729575b8bd96`.

Artifact: https://github.com/rastogivaibhav/graphenedb_v1/actions/runs/36780063690/artifacts/11127094635

## Gate

**PASSED for the existing R2 execution/schema/determinism gate.**

**INCOMPLETE for native transition mechanism coverage and readiness to score.**

The frozen scientific contract and candidate remain unchanged. Scoring remains unauthorized. No accuracy, comparative performance or capability claim is established.

## Next Authorized Activity

Complete the existing isolated native-transition remediation, pass its focused mechanical tests, then repeat the blind unscored campaign on that exact production head and inspect native transition coverage before advancing the score boundary.
