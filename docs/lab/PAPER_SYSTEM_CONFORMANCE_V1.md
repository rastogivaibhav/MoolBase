# GrapheneDB Paper → System Conformance Gate v1

Status: **active lab gate; evidence before external comparison**

## Purpose

Before GrapheneDB is compared with Jev or another external system, establish whether the implementation behaves consistently with the claims and invariants in its own current paper and specifications.

This gate does **not** replace GJ-Eval and does not modify its frozen semantics. External comparison is parked until this internal conformance gate is understood.

Public product boundary remains:

- **GrapheneDB** — persistent epistemic reasoning substrate and authoritative durable engine.
- **HypoKosh** — competing-hypothesis/proposal runtime.
- **DWM / bounded dialectic** — challenge, reopen and synthesis loop.

## Evidence statuses

Every claim receives exactly one status:

- **PROVEN** — implementation path exists and executable repository evidence demonstrates the stated behaviour.
- **IMPLEMENTED / UNPROVEN** — implementation exists, but current evidence is insufficient to establish the behavioural claim.
- **PARTIAL** — mechanism exists but is materially narrower than the claim/hypothesis.
- **MISSING** — no implementation path establishes the claimed mechanism.
- **CONTRADICTED** — observed implementation behaviour violates the stated invariant.
- **FUTURE WORK** — source document explicitly excludes the capability from current claims.

No aggregate “percent aligned” score is permitted: a single violated epistemic invariant may matter more than many implemented conveniences.

## Source hierarchy

1. `paper/main.tex` — current GrapheneDB technical preprint.
2. `paper/CLAIM_EVIDENCE_MATRIX.md` — current release claim boundary.
3. `docs/DIALECTIC_REASONING_V0.md` — bounded DWM/dialectic implementation contract.
4. `docs/GOVERNED_LEARNING_V0_SPEC.md` — governed HypoKosh/outcome-learning contract.
5. Code and executable tests — determine what actually exists; documentation never upgrades failing behaviour.

Earlier whitepaper claims that the current claim matrix explicitly demotes to future work remain future work unless separately re-established.

## Initial claim-to-system ledger

| ID | Claim / invariant | Required observable behaviour | Current repository mapping | Initial status | Proof required |
|---|---|---|---|---|---|
| C01 | Lineage identities remain distinct | graph route, source, evidence-family and derivation identities remain separately inspectable | `fiber_bundle` + lineage tests | IMPLEMENTED / UNPROVEN | executable invariant receipt |
| C02 | Known correlated evidence does not inflate independent support | adding a duplicate/shared-family path must not increase independent-support count | FiberBundle v2 / cross-dataset structural suite | IMPLEMENTED / UNPROVEN | metamorphic duplicate-family test |
| C03 | Material contradiction blocks final resolution | decisive opposition cannot silently produce `resolved` | epistemic controller / critic tests | IMPLEMENTED / UNPROVEN | contradiction trajectory |
| C04 | Alternatives survive convergence | unselected competing paths remain addressable after convergence | dialectic `discarded_paths` contract | IMPLEMENTED / UNPROVEN | convergence preservation test |
| C05 | Opposition is operational, not only textual | reopen targets influence a bounded subsequent expansion when enabled | dialectic runtime | IMPLEMENTED / UNPROVEN | reopen intervention trajectory |
| C06 | No silent promotion | reasoning cannot mutate stored origin/promotion state | dialectic safety contract; HypoKosh proposals remain `Hypothetical` | IMPLEMENTED / UNPROVEN | before/after durable-state hash + origin assertion |
| C07 | HypoKosh preserves proposal status | generated hypotheses remain hypothetical even when based on observed evidence | governed-learning GL-001/002 | IMPLEMENTED / UNPROVEN | proposal-origin test |
| C08 | HypoKosh reasoning is read-only | proposal/discriminating-test generation causes zero durable writes | GL-003 | IMPLEMENTED / UNPROVEN | restart-safe durable-state comparison |
| C09 | Promotion is governed | a policy/world-state change cannot activate without explicit approval metadata | GL-012–015 | PARTIAL | distinguish implemented policy promotion from still-future durable model-world promotion |
| C10 | Revision history is append-only | rollback/revision adds an event and does not erase prior decision | GL-014 | IMPLEMENTED / UNPROVEN | promote→rollback→restart trajectory |
| C11 | Reopening preserves inspectability | late contradiction can reopen search without deleting prior evidence/history | dialectic opposition + immutable bundles | PARTIAL | canonical late-counterevidence trajectory |
| C12 | Receipt explains governed outcome | selected target/status/evidence lineage/contradiction/residual uncertainty are reproducible | epistemic receipt | IMPLEMENTED / UNPROVEN | deterministic replay/hash test |
| C13 | Recovery is defect-specific and bounded | graph-searchable defects can re-expand; external/human evidence gaps do not cause blind widening | escape/recovery controller | IMPLEMENTED / UNPROVEN | paired defect-type tests |
| C14 | DWM synthesis cannot bypass epistemic gates | synthesis status follows governed evidence state and performs no durable promotion | dialectic synthesis + `durable_writes=false` | IMPLEMENTED / UNPROVEN | adversarial synthesis test |
| C15 | Autonomous external experiment loop exists | system independently executes discriminating tests against the world | explicitly excluded by current dialectic contract | FUTURE WORK | none for v1 |
| C16 | Hidden dependence is automatically discovered | unknown source dependence is inferred without configuration | explicitly disclaimed by paper | FUTURE WORK | none for v1 |
| C17 | Semantic truth/answer correctness is established | system improves general semantic correctness | explicitly H in claim matrix | FUTURE WORK | separate preregistered semantic evaluation |

## Canonical falsification trajectories

The first executable conformance suite should contain small deterministic trajectories rather than a broad leaderboard.

### T1 — Duplicate-family false convergence

1. Start with one admissible support path for H1.
2. Add graph-distinct evidence derived from the same evidence family.
3. Assert raw path count increases while independent-support count does not.
4. Assert H1 cannot cross a support gate solely because of the duplicate.
5. Preserve both paths for audit.

Falsifies C01/C02 if duplicate evidence is counted as independent corroboration or lineage disappears.

### T2 — Contradiction and safe non-convergence

1. Establish sufficient independent support for H1.
2. Add material opposition to H1.
3. Assert contradiction remains visible.
4. Assert final state is not silently `resolved` while blocking contradiction remains.
5. Assert the prior support remains inspectable.

Falsifies C03/C04.

### T3 — Late counterevidence → reopen → revise

1. Reach a governed provisional/resolved H1 state under sufficient evidence.
2. Introduce decisive late counterevidence plus a viable H2 path.
3. Run opposition.
4. Assert reopen targets affect subsequent bounded expansion.
5. Assert old evidence/history remains inspectable.
6. Assert final state revises to supported H2 or safely abstains/contests; it must not preserve H1 merely because H1 was previously selected.

This is the central Graphene + HypoKosh + DWM trajectory. Falsifies C03/C04/C05/C11/C14.

### T4 — Hypothesis proposal cannot become fact by generation

1. Generate a HypoKosh proposal from observed evidence.
2. Assert proposal origin remains `Hypothetical`.
3. Assert proposal generation performs no durable write.
4. Feed proposal into dialectic reasoning and assert synthesis cannot silently promote it.

Falsifies C06/C07/C08/C14.

### T5 — Governed promotion and rollback

1. Evaluate a candidate policy/decision.
2. Attempt activation without approval and require rejection.
3. Activate with explicit approval/approver/evaluation/event identity.
4. Roll back through a new append-only event.
5. Restart and assert current state plus complete history survive.

Establishes only the governed-learning/policy mechanism. It must **not** be used to claim durable DWM model-world promotion, which remains a separate gap.

### T6 — Deterministic receipt/replay

Replay identical evidence and configuration and require the same canonical governed receipt/hash, or an explicit documented nondeterministic field excluded from the canonical hash. Prior evidence and contradiction references must remain traceable.

## Immediate implementation priority

Do not add generic memory features. Build the conformance harness around T1–T6 using existing public APIs/tests wherever possible. Run it against the unmodified current implementation and preserve failures.

The first engineering delta after that run is selected by the highest-severity failed invariant, in this order:

1. silent promotion / history destruction;
2. contradiction incorrectly resolving;
3. correlated evidence inflating support;
4. failure to reopen/revise after decisive counterevidence;
5. non-reproducible receipts;
6. bounded-recovery mismatch.

## External research alignment (context, not proof)

Recent 2026 work independently increases the importance of these tests: BeliefShift evaluates contradiction detection and evidence-driven revision; BeliefMem retains multiple candidate conclusions under partial observability; MemTX separates memory writes from governed belief commits; and DeltaLogic isolates revision inertia after minimal evidence edits. These systems do not validate GrapheneDB. They make the conformance questions more relevant and raise the bar for claiming novelty.

## Exit from this gate

External head-to-head work may resume when:

1. T1–T6 are executable from a clean checkout;
2. every current-paper claim is mapped to code and evidence or explicitly classified as partial/missing/future;
3. all invariant failures are preserved in a machine-readable report;
4. the highest-severity failures have implementation issues rather than being explained away;
5. the paper/claim matrix is corrected if code evidence contradicts current wording.

Passing this gate demonstrates internal paper/system consistency only. It does not satisfy issue #25's independent validation or adoption exit conditions.
