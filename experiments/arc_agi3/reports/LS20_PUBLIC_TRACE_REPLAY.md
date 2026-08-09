# ARC-AGI-3 Experiment 0 — LS20 Public Trace Replay

## Status

**Real public-game evidence, offline replay. Not a live ARC/Kaggle score.**

The replay uses 31 observed LS20 level-7 action/outcome records from the public
`AgentNativeResearchLab/arc-agi3-ls20-agent-trajectories` dataset. The records
cover turns 344–374 of the recipes-only ablation arm and include action IDs,
changed-cell counts and panel-change labels.

The current Experiment-0 epistemic kernel was not given the external
`panel_changed` label when updating beliefs; that label is retained only for
post-hoc diagnosis.

## Result

- 31 real LS20 transitions replayed.
- All four legal actions received direct observed evidence.
- Hypotheses remained provenance-labelled `hypothetical`; zero silent promotion.
- The Lyapunov critic reached a low-energy equilibrium on its *current* metrics.
- The system detected contextual variability for several actions.

## Important failure discovered

The current perception model is too coarse for LS20.

It relies mainly on `changed_fraction`, so both four-cell locomotion events and
large 286-cell panel/control changes remain below the 25% "local" threshold.
Consequently the kernel strongly supports both `local_transform` and
`movement_like` for actions that are clearly producing qualitatively different
visual effects in the public trace.

The final critic can therefore report `equilibrium` even though the semantic
world model is inadequate. This is a **false epistemic equilibrium caused by an
underpowered state representation**, not a failure of the Lyapunov calculation
itself.

The selector also concentrates heavily on the least-tried/high-uncertainty
action once all actions have some evidence. It has no representation yet for:

- object identity / persistent entities;
- source-to-destination motion vectors;
- action-direction semantics;
- transformation/control tiles;
- key state;
- lock/door state;
- goal-directed navigation.

## Research implication

This is useful evidence. Graphene/HypoKosh can preserve alternatives,
contradictions and stability state, but the quality of its epistemic state is
bounded by the quality of perception/proposal generation. LS20 exposes exactly
the same boundary seen in earlier frozen GrapheneDB tests: governance of
hypotheses is not sufficient if the system cannot propose the right causal
variables.

## Next gate

Add a generic ARC visual transition extractor before changing the dialectic:

1. per-colour conservation across transitions;
2. removed-vs-added pixel centroids;
3. inferred translation vector `(dx, dy)`;
4. connected changed-region/component statistics;
5. distinguish translation from state/appearance transformation;
6. accumulate action-to-motion hypotheses (`ACTIONn -> up/down/left/right`);
7. retain non-motion events as candidate interaction/latent-state changes.

Then replay the same LS20 public trajectory and require the critic to *not*
claim semantic equilibrium while motion and interaction explanations remain
confounded.
