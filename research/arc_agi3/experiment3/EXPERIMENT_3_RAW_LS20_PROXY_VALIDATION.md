# Experiment 3 — Raw-Frame LS20 Goal Discovery and Active Affordance Exploration

## Status

**PASS for the source-derived Level-1 raw-frame proxy. Not yet an official ARC-AGI-3 score.**

Experiment 3 tests the next claim after Experiment 2: can the agent discover a useful state-changing affordance and a conditional goal from raw 64×64 frames while it controls the environment, without being given semantic labels such as “rotation pad”, “goal”, “switch”, or “key box”?

## Environment boundary

The test harness is a **source-derived LS20 Level-1 proxy**, not the official ARC engine. It uses public/source-derived Level-1 facts: a 64×64 observation, a 5×5 controlled entity, ACTION1–4 as up/down/left/right in 5-pixel increments, start at (34,45), a state-changing landmark around (19,30), and a target around (34,10). The proxy intentionally exposes only pixels, available action IDs, and `levels_completed`; those semantic roles and coordinates are not passed to the agent.

The proxy simplifies the full LS20 implementation. It models the Level-1 causal requirement as one state-changing interaction before successful target entry and contains neutral blocking geometry for navigation testing. Therefore these results are **mechanism evidence**, not an official benchmark result and not proof that the full LS20 environment is solved.

## Experiment-3 additions

1. **Step-size-independent action semantics** — learned motion vectors such as (-5,0) are normalised to directional semantics when planning.
2. **Controllable-entity discovery** — the moved colour/entity is inferred from repeated translation evidence.
3. **Compact landmark clustering** — sparse same-colour components are clustered into candidate visual landmarks; large/elongated structures are filtered as likely background/walls.
4. **Committed affordance exploration** — once a landmark is selected, the agent continues toward it rather than resampling an uncertain action every turn.
5. **Negative interaction evidence** — a blocked approach near a landmark is recorded as a failed experiment and the controller moves on instead of hammering the same action.
6. **Missing-direction discovery** — if a needed direction has not yet been mapped to an action, the agent deliberately probes an unknown action.
7. **Causal epochs** — after an observed state-changing interaction, previously inert landmarks become eligible for retesting because their effect may be conditional on the new state.
8. **Sparse-sprite hull contact** — goal predicates include conservative geometric hull contact in addition to visible-pixel overlap, preventing sparse sprites from hiding a real interaction.
9. **State frontier** — when no grounded landmark plan is available, the controller can prefer actions not yet tried from the current observed frame state.

All of these mechanisms are generic and operate on raw frames/action outcomes. The code contains no LS20-specific coordinates, colour IDs, or named mechanics.

## Single on-policy trace

The frozen Experiment-3 runner solved the proxy within the 80-action budget. A representative run used **20 actions**. The controller first learned motion, tested a visually salient landmark, discovered the state-changing landmark, reopened the candidate space in the new causal epoch, then reached the target and observed `levels_completed` increase.

After success, the highest-specificity supported goal hypothesis was:

```text
state_then_contact
  contact:hull:12:5
  context:prior_state_change
support = 1
contradiction = 0
confidence = 0.6667
origin = hypothetical
```

The weaker “terminal action” hypothesis accumulated multiple contradictions because ACTION1 had been used repeatedly without progress. No goal/mechanic hypothesis was silently promoted to observed/discovered truth.

## Seed robustness

Frozen evaluation: **20 deterministic agent seeds**, 80-action cap each.

| Condition | Solved | Solve rate | Mean actions (solved) | Min | Max | Learned state→contact | No silent promotion |
|---|---:|---:|---:|---:|---:|---:|---:|
| Full Experiment 3 | 20/20 | 100% | 17.6 | 14 | 20 | 20/20 | 20/20 |
| **No affordance exploration** | 0/20 | 0% | — | — | — | 0/20 | 20/20 |

The ablation disables `plan_affordance_exploration` but leaves the Experiment-3 perception, hypothesis/goal machinery, Lyapunov controller, adaptive turn floor and state-frontier mechanism active. In this proxy, state-frontier exploration alone did not solve any of the 20 runs within 80 actions.

This is evidence that **committed visual affordance exploration**, not merely random seed luck or the existing goal layer, caused the observed improvement in this controlled environment.

## Regression validation

```text
pytest -q
19 passed
```

The unit suite covers the earlier Experiment-1/2 contracts plus step-size-independent direction mapping. The separate frozen ablation script reproduces the 20-seed result and writes its full JSON evidence.

## Claim boundary

Supported now:

- the agent can infer motion primitives from raw frames;
- identify a controllable visual entity;
- commit to and test unlabeled visual landmarks;
- discover an unlabeled state-changing interaction;
- revisit candidate landmarks after that state change;
- reach an unlabeled target in the source-derived proxy;
- infer a `state_then_contact` goal hypothesis from the observed progress event;
- do so robustly across the tested seeds;
- and the affordance-exploration ablation materially changes success on this proxy.

Not supported yet:

- an official ARC-AGI-3 LS20 solve;
- any leaderboard score;
- all seven LS20 levels;
- transfer to other public ARC-AGI-3 games;
- proof that the Lyapunov critic itself improves score (that requires its own ablation);
- global convergence or AGI claims.

## Next gate

Run the same Experiment-3 `MyAgent` inside the **official `arc-agi` local LS20 environment**, preserving raw-frame-only inputs and an on-policy trace. If Level 1 succeeds, expand to multiple levels/games and then run the planned component ablations on the official environment before Kaggle submission.
