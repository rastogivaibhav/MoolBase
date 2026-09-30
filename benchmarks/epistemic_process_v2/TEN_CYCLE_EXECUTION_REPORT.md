# EP-PROCESS-V2 — Ten-Cycle Execution Report (Pre-Freeze)

## Provenance

V2 branched from sealed V1 score commit `3c008577739b83a94defc47c73275bf8ba76e76c`. V1 was not modified or rescored.

Successful unscored laboratory run: `36682092122`.
Evidence artifact: `11081768858`.
Artifact digest: `sha256:ddf3b5875013d476f98408c911820bc81c36b4ea1d319564df27893a6f3d111e`.

An earlier run failed the replay gate because the harness compared artifacts containing different recorded seed metadata. Replay was corrected to use the identical seed. No production reasoning logic was changed in response.

## Cycle status

1. **G0 forensics — PASS.** G0 is intentionally evidence/provenance without hypothesis selection. V1 projected native abstention as open.
2. **G1 forensics — PASS.** Native Revision represents intra-call re-expansion, while V1 cross-step H1→H2 changes were separate fresh decisions.
3. **G2 forensics — PASS.** DWM Challenge/Reopen was active, but native Revision required an answer change inside the same call.
4. **Preregistration — PASS.** Hypotheses, metrics, semantic states, failure policy and task-bank requirements were frozen before V2 execution.
5. **Measurement implementation — PASS.** V2 preserves G0 abstention, temporal observations, reopen progress, explicit revocation, and irrelevant-noise semantics without modifying production reasoning.
6. **Task bank — PASS.** 66 deterministic episodes across 11 strata.
7. **Causal ablation — PASS mechanically; mixed scientifically.**
8. **Stress/adversarial — PASS.** M1–M11 pass; 500 deterministic monkey + 100 mutation episodes pass; three 100-episode multiseed runs show zero failures, crashes or nondeterminism; identical-seed replay passes; malformed input and unknown revocation fail closed.
9. **Realistic deterministic pack — PASS mechanically; mixed scientifically.** 16 scenario-shaped episodes across incident, policy, catalog, research, security, operations and knowledge contexts.
10. **Immutable score — score infrastructure prepared; authorization remains locked until an exact merged candidate is hashed into a separate FROZEN manifest.**

## Main 66-episode findings

| Observation | B0 | G0 | G1 | G2 |
|---|---:|---:|---:|---:|
| Runtime/adapter failures | 0 | 0 | 0 | 0 |
| Overall terminal accuracy | 81.82% | 18.18%* | 81.82% | 81.82% |
| Coverage | 72.73% | 0%* | 100% | 100% |
| Abstention rate | 27.27% | 100%* | 0% | 0% |
| Temporal positive-oracle sensitivity | 25% | 25% | 75% | 75% |
| Temporal negative-control specificity | 100% | 100% | 100% | 100% |
| Native Revision transitions | 0 | 0 | 0 | 0 |
| Challenge events | 0 | 0 | 0 | 246 |
| Reopen transitions | 0 | 0 | 0 | 246 |
| Effective expanded calls | 0 | 0 | 0/156 | 0/246 |

* G0 terminal-answer metrics are non-primary because G0 has no hypothesis selector.

G0 retained **774/774 expected active evidence references** and **774/774 evidence-family identities**.

For G1/G2, all **36/36** synthetic H1→H2 target changes succeeded and all **18/18** temporal negative controls remained unchanged. However, all **12/12** cases requiring abstention after insufficient evidence failed: G1/G2 selected a hypothesis instead.

G2 emitted **246 Challenge** and **246 Reopen** events, but **0/246** expanded calls changed the bundle/frontier under the preregistered effectiveness definition. It produced no terminal-accuracy or temporal-sensitivity improvement over G1.

Across the 18 temporal negative-control episodes, G2 emitted reopens in **18/18 episodes** (48 total) despite no hypothesis change.

## Realistic 16-episode findings

- G0 evidence-reference recall: **188/188 (100%)**.
- G0 evidence-family integrity: **188/188 (100%)**.
- G1/G2 terminal accuracy: **87.5%**.
- G1/G2 H1→H2 cases: **10/10**.
- G1/G2 required-abstention cases: **0/2**.
- G1/G2 negative-control specificity: **100%**.
- G2: **56 challenges, 56 reopens, 0 effective expanded calls, 0 native revisions**.

These are deterministic scenario-shaped workloads, not external production data.

## Preregistered claim ledger before score freeze

| Claim | Status | Evidence |
|---|---|---|
| H-G0-1 evidence/provenance persistence | SUPPORTED_WITH_SCOPE | 100% active-reference recall and family integrity in both packs; deterministic replay passed. |
| H-G1-1 governed selection | SUPPORTED_WITH_SCOPE | Selection is stable and false-resolution rate is zero, but required abstention fails. |
| H-G1-2 temporal change after decisive evidence | SUPPORTED_WITH_SCOPE | Designed H1→H2 changes succeed; transitions to abstention fail. |
| H-G2-1 useful Challenge/Reopen beyond G1 | CONTRADICTED | No G1→G2 accuracy/sensitivity gain and 0 effective expanded calls. |
| H-G2-2 avoid unnecessary churn | CONTRADICTED | All 18 negative-control episodes generated G2 reopens. |
| H-ABS-1 appropriate abstention | CONTRADICTED | G1/G2 fail all 12 synthetic and both realistic required-abstention cases. |

## Freeze decision

The mixed result is the evidence. No production fix is permitted before the V2 score freeze. A future repair must be a separately versioned experiment.
