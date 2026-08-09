# ARC-AGI-3 Epistemic World Model — Experiment 1 Validation

Date: 2026-08-09

## Goal

Fix the failure exposed by the first LS20 replay: the earlier ARC projection treated changed-cell fraction as the main perceptual variable, causing very different effects to collapse into the same hypothesis family and allowing a low-energy Lyapunov state to be mistaken for a sufficiently understood world.

Experiment 1 adds generic pixel-level world representation plus an adaptive minimum-turn floor.

## Implemented

### 1. Object / transition understanding

`agent/my_agent.py` now includes:

- 4-neighbour same-colour connected-component extraction;
- background inference from the dominant colour;
- normalized component-shape signatures;
- same-colour / same-shape component matching across frames;
- integer translation-vector extraction;
- dominant motion-vector inference;
- per-colour count conservation;
- changed-region counting;
- generic effect classes: `no_effect`, `translation`, `local_transform`, `multi_region_transform`, `large_structured_change`, `global_transform`.

The classifier considers absolute changed-cell count as well as changed fraction. Consequently, an LS20-like 286/4096 event is no longer classified as ordinary tiny movement simply because it occupies only ~7% of the screen.

### 2. Mechanic hypotheses

Actions can now accumulate hypotheses such as:

- movement / translation primitive;
- concrete motion vector, e.g. `move_-1_0` or `move_0_1`;
- localized transformation;
- structured interaction / latent-state change;
- no visible effect;
- context dependence;
- goal progress.

All explanatory hypotheses remain `origin=hypothetical`; repeated use never silently upgrades them to discovered truth.

### 3. Lyapunov vs epistemic admissibility

Low Lyapunov energy is no longer sufficient to declare the world model resolved.

The controller separately requires:

- environment-derived minimum-turn floor reached;
- adequate action coverage;
- sufficiently high representation quality;
- bounded perceptual/retrieval noise;
- no unresolved material contradiction.

This introduces control states including `under_observed`, `evidence_limited`, `learning`, `reopen`, and `stable` while retaining the Lyapunov trajectory regime independently.

### 4. Adaptive minimum-turn policy

Parameters:

- `ARC_MIN_TURNS_BASE` default `12`
- `ARC_MIN_TURNS_MAX` default `64`
- `ARC_STABLE_DWELL` default `4`
- `ARC_ALLOW_EARLY_STOP` default `0`
- `ARC_MAX_ACTIONS` default `80`

The adaptive minimum increases with observed action count, effect-class diversity, context-dependent action behaviour, perception uncertainty, and level depth. Competition-safe default is to never stop early without WIN; optional early-stop mode can be enabled for controlled local experiments and still cannot stop before the adaptive floor plus stable-dwell requirement.

## Automated tests

Command:

```text
pytest -q
```

Result:

```text
14 passed in 0.05s
```

Coverage includes object segmentation, translation/direction detection, structured-change classification, directional hypothesis accumulation, context variation, adaptive minimum-turn growth, false-equilibrium blocking, low-quality-evidence blocking, balanced exploration, early-stop floor, MyAgent contract, and the full Experiment-1 research contract.

## Frame-level controlled unknown-world run

A generic 8x8 world was used only as a controlled mechanism test. The agent was not given action semantics. It received raw before/after frames.

Result after 28 evidence-bearing turns:

```text
ACTION1 trials: 6 -> learned move_-1_0, support=6, confidence=0.875
ACTION2 trials: 6 -> learned move_1_0,  support=6, confidence=0.875
ACTION3 trials: 6 -> learned move_0_-1, support=6, confidence=0.875
ACTION4 trials: 5 -> learned move_0_1,  support=5, confidence=0.857
ACTION5 trials: 5 -> learned local transformation
```

Final controller state:

```text
Lyapunov energy:      0.0
Lyapunov regime:      equilibrium
Control status:       stable
Evidence admissible:  true
Adaptive min turns:   21
Observed turns:        28
Minimum floor met:    true
Voluntary stop ready: true
Stable dwell:          4 turns
Silent promotions:    0
```

This validates the new pixel-to-object-to-motion path and the minimum-turn gating mechanism. It is not an ARC leaderboard result.

## LS20 public metadata replay — same 31-record slice as Experiment 0

The same public L7 slice (turns 344-374 in the public viewer) was replayed to test whether the earlier false-equilibrium conclusion is still possible.

Experiment 0 ended with:

```text
final energy: 0.00976
regime: equilibrium
```

and lacked a separate evidence-admissibility gate.

Experiment 1 classifies the 31 observed transitions as:

```text
large_structured_change: 19
local_transform:          12
```

Final state:

```text
final Lyapunov energy:    0.0198565
Lyapunov regime:          equilibrium
control status:           under_observed
evidence admissible:      false
adaptive minimum turns:   44
observed turns:            31
minimum floor met:        false
silent promotions:        0
```

This is the intended fix: the numerical Lyapunov trajectory may look settled, but the epistemic controller refuses to treat the model as resolved because the observed environment requires more evidence and the replay's perception source is coarse metadata rather than raw frame pairs.

## Important interpretation boundary

The LS20 replay is off-policy: it feeds transitions chosen by another agent. Therefore the `suggested_next_action` counts from this replay are diagnostic only and must not be interpreted as a causal evaluation of our action policy. Balanced action selection is validated in the on-policy controlled test; a real policy comparison requires live ARC execution or a branchable environment trace.

The public LS20 dataset contains frame-level recordings, but this specific replay intentionally uses the public viewer metadata slice so it is directly comparable with the first Experiment-0 replay. Directional motion learning is validated from raw pixel frames in the controlled mechanism test; the next stronger validation is a live/local ARC run or a raw LS20 frame-pair replay with action-aligned frames.
