# Reasoning modes and compact receipts

GrapheneDB separates ordinary answer recovery from optional secondary research.

## Default: recovery only

The complete runtime performs one bounded pass. It re-expands only when the
critic diagnoses a graph-searchable evidence defect, such as:

- a missing causal hop;
- insufficient independent evidence;
- unresolved contradiction;
- temporal mismatch;
- retrieval noise;
- a relevant minority path.

Tasks that require an external semantic verifier or human evidence do not cause
repeated graph search because widening the graph cannot satisfy them.

## Optional: opposition research

Set:

```cpp
RuntimeOptions options;
options.enable_opposition_research = true;
```

to perform secondary or tertiary research after opposition identifies competing
hypotheses. Opposition `reopen_nodes` are converted into downstream anchor
candidates for the next expansion; the root node itself is not counted as a
zero-hop confirmation.

## Frontier-aware stopping

A completed FiberBundle can remain unchanged while reverse traversal reaches
new intermediate nodes. The runtime therefore distinguishes:

- completed-bundle change;
- traversal-frontier progress;
- bounded unchanged-bundle patience;
- genuine no-progress termination;
- limit cycles and oscillation.

The default unchanged-recovery patience is two rounds and all recursion remains
capped at three cycles.

## Storage policy

FiberBundle is primarily an ephemeral, deterministic reasoning projection. A
long-lived integration should persist compact epistemic receipts containing:

- selected conclusion and governed status;
- source/evidence-family/derivation references;
- selected path and bundle hashes;
- critic energy and admissibility summary;
- remaining contradiction and uncertainty;
- verifier and policy versions.

Full bundle and per-cycle workspace data should be retained only for explicit
audit, debugging or research runs. This avoids storing source data, indexes and
complete repeated reasoning states as separate permanent copies.
