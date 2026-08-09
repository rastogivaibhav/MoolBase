# ARC-AGI-3 Epistemic Agent — Experiment 0 Local Validation

Date: 2026-08-09

## Baseline used

- GrapheneDB research branch reference: `fix/fiberbundle-v2-lyapunov`
- ARC interface reference: official `arcprize/ARC-AGI-3-Kaggle-Starter` `MyAgent` contract
- Local package: `arc_agi3_graphene_experiment`

## Implemented

- direct action→frame transition evidence with `observed` provenance;
- competing action-mechanic hypotheses retained as `hypothetical`;
- divergent hypothesis families (`no_effect`, `local_transform`, `global_transform`, `stateful_context`, `movement_like`, goal-progress when observed);
- bounded 11-coordinate Lyapunov-style epistemic energy using the latest GrapheneDB default weights/targets;
- critic regimes: insufficient history, descending, marginal, diverging, equilibrium, oscillating, limit cycle;
- forced exploration while legal actions remain untested;
- contradiction/context sensitivity from inconsistent action effects;
- empirically observed level progress can switch the policy from exploration to exploitation;
- complete per-action reasoning receipt attached to ARC `GameAction.reasoning`;
- no silent promotion from hypothetical explanation to discovered truth.

## Tests

```text
python -m pytest -q
.......                                                                  [100%]
7 passed in 0.03s
```

Coverage includes frame-delta extraction, multi-hypothesis generation, repeated-evidence behaviour, contradiction/reopening signal, information-seeking selection, official `MyAgent` reset/decision contract, and the frozen Experiment-0 research contract.

## Synthetic unknown-world run

Ten decision cycles were executed against a small hidden-mechanics grid world that the kernel was not given in advance.

```text
evidence_count      = 10
actions_tested      = [1, 2, 3, 4, 5]
hypothesis_count    = 24
no_silent_promotion = true
final_energy        = 0.0068653061
final_regime        = equilibrium
```

The synthetic run is **not an ARC leaderboard result**. It proves only that the epistemic control loop executes and produces auditable evidence/hypothesis/critic/action traces before we spend ARC evaluation budget.

## Remaining gate

Run this exact agent inside the official ARC-AGI-3 local/public environment and then the Kaggle rerun. That run will produce the first genuine ARC-specific score and interaction-efficiency evidence for the paper.
