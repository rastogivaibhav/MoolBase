# ARC-AGI-3 Experiment 2 — Goal Discovery Validation

Date: 2026-08-09

## Objective

Test whether the ARC epistemic agent can learn not only **what actions do**, but also **what conditions appear to produce progress**, without hard-coding a goal. The goal layer must preserve competing explanations, use negative evidence, prefer more specific relational/conjunctive explanations over action repetition, reuse a supported goal on a new layout, and reopen the goal when a supposedly sufficient condition later fails.

## Implementation added

- `GoalHypothesis`: provenance-bearing, support/contradiction-scored candidate goal predicates.
- Retrospective falsification: when a new goal hypothesis is created after a success, earlier matching non-success observations are replayed as counterexamples.
- Relational contact predicates from raw frames using the previously learned action/vector model.
- `state_then_contact` conjunctions so a state-setting interaction can be distinguished from the final delivery/contact condition.
- State-changing affordance memory (`interaction_targets`) kept separate from goal truth.
- Cross-layout goal planner: satisfy a learned state precondition first, then move toward the learned relational goal.
- Goal-aware control status: a mechanically stable trajectory remains `goal_unknown` until progress grounds at least one goal hypothesis.
- Goal uncertainty contributes to the adaptive minimum-turn budget through `ARC_GOAL_TURN_BONUS_MAX` (default 8).
- Goal contradiction triggers reopening rather than silent persistence.

## Validation suite

`pytest -q`

Result: **18 passed, 0 failed**.

Coverage includes prior Experiment-1 perception/transition/minimum-turn contracts plus:

1. action/contact-only goal explanations are weakened by earlier negative evidence;
2. a `state_then_contact` explanation survives when contact before state-setting did not progress;
3. the goal planner transfers the conjunction to a new layout;
4. no voluntary stop is allowed while the goal remains unknown;
5. a later counterexample demotes a previously supported goal and triggers reopening;
6. goal hypotheses remain `hypothetical` — no silent promotion.

## Controlled two-level goal discovery experiment

Level 1 contains three important observations:

- touching the delivery target **before** setting latent state does not progress;
- touching a different object changes a remote panel but does not progress;
- touching the delivery target **after** that state change increments the level.

The agent receives no explicit rule. It first learns the four movement directions from frame transitions, then observes the above evidence.

Top hypothesis after Level 1:

```text
family: state_then_contact
predicates:
  contact:overlap:1:2
  context:prior_state_change
support: 1
contradiction: 0
confidence: 0.6667
origin: hypothetical
```

Competing explanations were retained and weakened by counterevidence: target contact alone had an earlier negative example; the final action had many non-progress examples; prior state change alone was followed by many moves without progress.

### Cross-layout transfer

Level 2 relocates the player, state-changing object and delivery target. The agent controls this level on-policy through `select_action()`.

- Level 2 solved: **YES**
- Steps: **11**
- Goal-directed steps: **11 / 11**
- Silent goal promotion: **0**

## Goal falsification / reopening

A supported `state_then_contact` goal began at confidence `0.6667`. The identical supposedly sufficient condition was then observed without progress.

```text
support:       1
contradiction: 1
confidence:    0.5000
origin:        hypothetical
reopening:     triggered
```

## Public LS20 milestone sanity check

A structural replay was built from public LS20 L1 milestone evidence. It is **not raw-frame inference and not a live ARC score**. The public trace describes reaching/pushing at the key-box before alignment without winning, stepping on a switch and changing the panel without winning, and returning to the key-box after the state-setting interaction and advancing the level.

The goal layer ranked the state-change+key-box-contact conjunction above key-box contact alone, prior state change alone and final-action repetition.

## Adaptive minimum-turn interaction

Goal uncertainty now contributes an additional bounded term:

```text
ARC_GOAL_TURN_BONUS_MAX=8
```

The adaptive floor can therefore rise when multiple viable goal hypotheses survive. It remains bounded by `ARC_MIN_TURNS_MAX` and does not override an actual WIN/level-progress signal.

## Claim boundary

Established here: conditional goal discrimination in a controlled world, negative-evidence backtesting, cross-layout transfer, contradiction-driven reopening, and LS20 structural consistency.

Not yet established: autonomous discovery of the complete LS20 goal from raw LS20 frames, live public-game success, full-benchmark generality, or leaderboard improvement.
