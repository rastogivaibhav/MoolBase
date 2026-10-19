# MOOL-POLYMATH-F10B-001 — Factorial graph topology × query mode diagnostic

**New preregistration, 19 October 2026, written after F10's negative forensics and BEFORE F10B code or results.** This is a new experiment, not a retroactive rescue of F9 or F10 findings. Original F10 receipt at SHA `3a72a4825a6c5d759f3993c693594e57bc5c4515` remains immutable.

## Source-derived root causes to test
The native `GrapheneEvidenceExpander::expand` terminates a path when the current node is `root=true` (before incoming-edge traversal). F9 authored the incident AND all three hypotheses as roots, yielding two zero-edge paths rather than traversing its contradiction edges. In `edge_allowed`, `QueryMode::Empirical` ignores `EdgeOrigin::Hypothetical` edges; all F9 edges are explicitly hypothetical. Thus the original study cannot measure admissible empirical opposition.

## Frozen factorial design
Take **revision stage only**, with exactly three hypotheses and six project-authored synthetic hypothetical edges (including two contradictions). Execute four configurations, each across the same three G0/G1/G2 real native runtime ablations (12 executions total):
1. ORIGINAL topology (incident and hypotheses roots, incident → hypothesis edges) + Empirical mode.
2. ORIGINAL topology + Theoretical mode.
3. CORRECTED topology (hypotheses are roots, incident nonroot, hypothesis → incident edges) + Empirical mode.
4. CORRECTED topology + Theoretical mode.

The corrected direction is explicitly a **test topology hypothesis**, not a recommendation to rewrite production data. Exact node texts, synthetic source status, hypothesis labels, relative confidence, G0/G1/G2 toggles and query vector remain fixed. Theoretical mode may traverse hypothetical claims, but **must never upgrade them to verified evidence or external causal truth**.

## Measurements and gates
For each actual native run, record persisted contradiction-edge IDs and intersection with retrieved FiberBundle path edge IDs, retrieved root/path count, support/opposition role counts, `eligible_for_opposition` count, eligible correlation-group representative count, DWM `oppose` invocation, challenge/reopen, governed status and ModelWorld writes. Record negative results; do not lower evidence-quality/independence thresholds. Hypothetical-only paths lacking authenticated critical-edge evidence may remain **ineligible**, in which case zero challenge is expected.

Acceptance is **12/12 native executions**, exact source and option guards, **0** empirical truth promotions and an interpretable attribution of F9 zero-opposition. Do NOT score selected causes as empirically correct or claim autonomous revision. A future real-outcome experiment still requires independent ground truth and authenticated, rights-cleared observations.
