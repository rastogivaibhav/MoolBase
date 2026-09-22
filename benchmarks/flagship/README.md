# GrapheneDB flagship epistemic proof

This is the public, deterministic demonstration for issue #33.

The question is deliberately narrower than “can an agent answer?”:

> **When has the evidence process earned convergence, and when should more retrieved material *not* increase confidence?**

## Run it

From a clean checkout:

```bash
python3 scripts/run_flagship_proof.py
```

No network, hosted model or external service is required.

The command builds and executes `graphenedb_epistemic_flagship_demo`, validates the canonical adversarial contract and writes:

```text
reports/flagship-proof/receipt.json
reports/flagship-proof/summary.md
reports/flagship-proof/scenario_manifest.json
reports/flagship-proof/raw_output.txt
```

The canonical **mechanism** receipt hash is:

```text
36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef
```

The mechanism hash covers the frozen scenario and deterministic mechanism outputs, but deliberately excludes commit and host environment. The receipt stores the exact commit separately and derives a second provenance hash from `commit + mechanism receipt hash`. This avoids tying the scientific receipt to GitHub's synthetic pull-request merge commit while preserving exact source provenance.

Expected status transitions include:

- Phase A: 2 raw paths, 1 independent family, insufficient independent support;
- Phase B: H1 selected, H2 retained, 2 reopen targets;
- Phase C: contradiction blocks resolution and evidence is inadmissible for final resolution;
- Phase D: bounded reopen, zero durable writes, 3 depth-recovery rounds with semantic candidates fixed at 1;
- Phase E: independent support present and contradiction blocker cleared, while prior Phase C bundle identity is retained.

Host platform metadata is retained for debugging but excluded from the mechanism receipt hash.

## Scenario

The scenario tracks two explanations for checkout timeouts:

- **H1:** a deployment-induced connection-pool change;
- **H2:** a traffic spike.

It proceeds through five controlled phases.

### A — correlation is not corroboration

Two graph-distinct support paths cite the same evidence family. Raw path count rises to two, but independent evidence-family support must remain one.

### B — the alternative survives

H1 gains a second genuinely independent source while H2 has separate admissible support. Selecting H1 must not erase H2; opposition must retain it as a challenge/reopen target.

### C — correct refusal of final resolution

Material opposition is added against H1. GrapheneDB may retain a candidate answer for audit, but the successful outcome is that **final resolution is blocked**: contradiction remains inspectable and the evidence is inadmissible for final resolution.

This is the negative control. Candidate selection is not the same thing as epistemically earned resolution.

### D — challenge and bounded reopen

A durable GrapheneDB fixture exercises the DWM/bounded-dialectic path. Opposition must request re-expansion, a reopened bundle must be produced, and the read-only DWM contract must perform zero durable writes.

A second hidden-chain fixture exercises HypoKosh recovery. A four-edge chain starts at `max_hops=1` and must advance `1→2→3→4` over three rounds while semantic candidates remain fixed at one. This proves that missing-hop repair advances the causal frontier rather than silently turning into broad retrieval.

### E — evidence changes the governed state

A new independent controlled-reproduction family is introduced and the contradiction blocker is removed in the controlled fixture. Earlier Phase C bundle identity remains in the final history record.

This is **not** a claim of durable cross-run DWM belief promotion/revision.

## Product mapping

- **GrapheneDB** — persistent evidence/causal substrate, lineage-aware FiberBundles, admissibility and receipts.
- **HypoKosh** — competing-hypothesis and defect-specific recovery runtime.
- **DWM** — bounded challenge, reopen and synthesis loop.

## How to falsify it

Useful attacks include:

1. duplicate a support route while keeping the same source/evidence family;
2. remove a genuinely independent family;
3. add material contradiction;
4. reorder deterministic path/evidence insertion;
5. alter the bounded depth/search budget.

Please report any case where GrapheneDB silently promotes a conclusion, loses contradiction, counts correlated evidence as independent support, broadens retrieval without the receipt showing why, or produces an unexplained canonical receipt change.

## Claim boundary

This demo supports controlled evidence for:

- known-lineage de-correlation;
- preservation of competing alternatives;
- contradiction-aware non-convergence;
- bounded operational reopen;
- defect-specific frontier recovery;
- deterministic inspectable receipts.

It does not establish:

- semantic truth;
- automatic hidden-dependence discovery;
- autonomous scientific discovery;
- durable cross-run DWM belief promotion/revision;
- general external-system superiority;
- enterprise readiness.

The frozen machine-readable scenario is `scenario.json`. The executable and runner contain assertions for the same invariants; CI publishes their output rather than replacing it with a marketing score.
