# ARC-AGI-3 Experiment 4 — Source-faithful LS20 Level-1 validation

Date: 2026-08-09

## Objective

Take Experiment 3 beyond the simplified LS20 proxy and expose it to a substantially more faithful Level-1 reconstruction based on the published `ls20-9607627b` source geometry/mechanics. The agent receives only raw 64x64 frames, available actions and level progress; no semantic labels are provided for the player, changer, target, walls or goal logic.

This is **not an official ARC scorecard run**. The current execution container cannot install the official ARC toolkit because outbound package/GitHub access is blocked, and the repository's GitHub-hosted Actions path did not start the prepared run. No official score is claimed.

## Published Level-1 structure reproduced

- 64x64 grid.
- Player start `(34,45)`.
- ACTION1/2/3/4 = up/down/left/right in 5-pixel increments.
- Rotation changer `(19,30)`.
- Goal `(34,10)`.
- Start shape/color already match goal; start rotation 270°, goal rotation 0°.
- StepCounter 42; StepsDecrement 1.
- Wall and target geometry transcribed from the published `ls20-9607627b` source.
- Source-faithful rendering includes player colours, changer, target frame/shape, player-state icon, step counter/lives UI and goal-match cue. Large decorative chrome is approximated rather than copied byte-for-byte.

## Unchanged Experiment-3 run

- Solved: **NO**
- Actions: **80/80**
- Levels completed: **0**
- Final latent rotation: **270°**

### Failure diagnosis

1. The step counter changes every action, so full-frame hashes made each turn appear to be a new state.
2. Small HUD changes caused ordinary movement to be classified as `local_transform` rather than translation.
3. Same-colour UI/goal/player elements made colour-only mover identity ambiguous.
4. State-frontier exploration therefore repeatedly re-explored effectively identical controllable states.

The simplified raw-frame proxy had hidden this representation failure.

## Generic fixes

No LS20 coordinates or semantic labels were added to the agent.

1. **HUD-tolerant transition decomposition** — dominant translated components remain movement when colour-count drift is small; larger residual changes become `translation_with_state_change`.
2. **Tracked controllable-object identity** — track the component that actually translated instead of aggregating every object sharing its colour; cluster sparse glyph fragments locally.
3. **Canonical state-frontier key** — use tracked mover position plus causal epoch rather than the whole-frame hash, so counters do not create fake novel states.
4. **Primary-axis primitive discovery** — if a committed landmark requires an unknown dominant-axis control, probe the unknown control before oscillating along a secondary axis.
5. **Tracked-mover contact reasoning** — goal/contact predicates are derived from the tracked mover rather than all same-colour objects; `translation_with_state_change` can ground a state-changing affordance.

## Rerun

- Solved: **YES**
- Actions: **67/80**
- Levels completed: **1**
- Final position: `(34,10)`
- Final rotation: **0°**
- Lives remaining: **2**
- Step-counter remaining: **20**

The agent discovered and visited the rotation-changing affordance, retained its learned model across a life reset, revisited the causal structure, then reached the target with the required latent state.

The strongest terminal goal candidates are now `state_then_contact` conjunctions with support=1 and contradiction=0. Multiple target contact colours survive because the visible target is composed of several coloured components; object-level relational compression is still needed.

## Regression

- Existing Experiment-3 unit/regression suite: **19/19 passed**.
- Simplified raw-frame LS20 proxy: **solved in 20 actions** after the fixes.
- Source-faithful Level-1 reconstruction: **solved in 67 actions**.

## Claim boundary

Established: the previous full-frame state representation fails under source-faithful HUD drift; a generic controllable-state abstraction fixes it; the revised agent solves the source-faithful LS20 Level-1 reconstruction within the official Agent 80-action ceiling; persistent experience survives a life reset and contributes to success.

Not established: execution through `arc_agi.Arcade()`, an official scorecard/leaderboard score, complete seven-level LS20 success, or generality across the public game set.