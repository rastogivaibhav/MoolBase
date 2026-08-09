# ARC-AGI-3 Epistemic World Model — Experiment 0

This is the first Kaggle-compatible ARC-AGI-3 projection of the GrapheneDB / HypoKosh / Dialectical Model World research line.

## Research contract

The first milestone is intentionally small. The agent must:

1. observe an action consequence;
2. preserve it as direct evidence;
3. generate multiple structurally different hypotheses about the mechanic;
4. compute an epistemic-stability/Lyapunov state;
5. choose another action because it reduces uncertainty / discriminates hypotheses;
6. update support and contradiction from the resulting observation;
7. never silently promote a hypothesis to discovered truth.

`agent/my_agent.py` is self-contained and follows the official ARC-AGI-3 Kaggle Starter `MyAgent` contract.

## Graphene/HypoKosh mapping

| Existing research component | ARC Experiment-0 projection |
|---|---|
| Temporal/event memory | `TransitionEvidence` |
| FiberBundle / non-convergence | per-action competing `MechanicHypothesis` set |
| Divergent discovery | structurally distinct action-mechanic families |
| LyapunovCritic | 11-coordinate bounded epistemic energy + regime |
| Corrective escape | information-seeking action selection |
| Opposition | contradiction is retained and forces reopening/escape |
| Experiment/Outcome feedback | every ARC transition updates support/contradiction |
| Provenance guard | observed evidence and hypothetical explanations remain separate |

The default Lyapunov weights and targets track the latest GrapheneDB C++ critic defaults on `fix/fiberbundle-v2-lyapunov`.

## Run tests

```bash
python -m pytest -q experiments/arc_agi3_epistemic/tests
```

## Put into official Kaggle starter

Copy `agent/my_agent.py` over the official starter's `agent/my_agent.py`, then use the official flow:

```bash
make setup
make play-local GAME=ls20
make submit
```

A real ARC score still requires the ARC runtime / Kaggle competition environment. This package validates the agent logic and research contract without pretending a synthetic test is an ARC leaderboard result.
