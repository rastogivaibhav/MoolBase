# Lyapunov Critic on 2WikiMultiHopQA — Run 1

Date: 28 July 2026  
Branch: `benchmark/2wiki-lyapunov`  
Source head: `5e392caa43f62a279c03ea2e42da294059d61616`  
Workflow run: `30398461068`  
Artifact: `8703769040`  
Artifact digest: `sha256:bb6e3cb0608d733d0bf159d4b031d8ff9f1e419a7799c9b2e9a49dc729abd6b9`

## Dataset sample

- Dataset: `framolfese/2WikiMultihopQA`, an unchanged schema repackage of the original dataset.
- Split: validation.
- Deterministic sample: 1,000 examples.
- Seed: `20260728`.
- Question types: 410 compositional, 248 comparison, 223 bridge-comparison and 119 inference.
- Temporal coordinate: not tested because the dataset does not contain dependable event-validity timestamps.

## Execution evidence

The workflow:

1. fetched the deterministic public 2Wiki sample;
2. compiled `bench_2wiki_lyapunov.cpp` directly with the materialised `src/fiber_bundle.cpp` and `src/stability_critic.cpp`;
3. ran all 1,000 examples through `graphene::FiberBundleBuilder` and `graphene::LyapunovCritic` in empirical mode;
4. produced per-example CSV and aggregate JSON/Markdown;
5. rebuilt and passed the existing direct `graphenedb_lyapunov_critic_tests` target;
6. uploaded the complete sample metadata and result evidence.

The benchmark workflow completed successfully. The broader GrapheneDB CI was launched separately by the pull request and is not used as a substitute for these critic results.

## Results

| Diagnostic | Result | Initial target | Verdict |
|---|---:|---:|---|
| Missing-hop energy above gold | 100.0% | >=90% | Pass |
| Contradiction energy above gold | 0.0% | >=90% | Fail |
| Same-source duplicate not rewarded | 0.0% | >=98% | Fail |
| Irrelevant distractor not rewarded | 0.0% | >=95% | Fail |
| Missing → gold trajectory monotonic | 100.0% | >=90% | Pass |

The combined diagnostic gate failed.

## Mean Lyapunov energy

| Condition | Mean energy | Stable rate |
|---|---:|---:|
| Gold evidence chain | 0.133082 | 0.0% |
| Missing hop | 0.321205 | 0.0% |
| Contradictory alternative | 0.028737 | 0.0% |
| Same-source duplicate | 0.000000 | 100.0% |
| Irrelevant distractor | 0.000000 | 100.0% |
| Structurally complete wrong path | 0.133082 | 0.0% |

## Findings

### 1. Missing-evidence response works

Removing a gold hop, lowering provenance and marking retrieval truncated raised energy in every tested example. The controlled repair trajectory was monotonically non-increasing in every example.

However, none of the repair trajectories reached the configured equilibrium goal or convergence certificate because the complete 2Wiki gold chain normally remains a single canonical path with high pattern lock and limited independent corroboration.

### 2. Contradiction is under-penalised

Adding a fully sourced contradictory path reduced mean energy from `0.133082` to `0.028737` in every example. The contradiction penalty was overwhelmed by the apparent gains in diversity, degeneracy and pattern-lock reduction.

This means the current quadratic energy does not preserve the required ordering:

```text
complete consistent evidence < contradictory evidence
```

### 3. Same-source duplicate paths are falsely rewarded

A path with the same evidence source IDs but a different edge sequence was counted as an additional independent path in 100% of examples. It reduced pattern lock in 100% of examples and drove energy to zero, causing a 100% stable classification rate.

The immediate cause is in `FiberBundleBuilder`: its independence key includes both source lineage and edge sequence. Different edges from the same sources therefore become separate independent lineages.

### 4. Irrelevant distractors are falsely rewarded

Adding a fully sourced irrelevant path reduced pattern lock in 100% of examples and drove energy to zero, again producing a 100% stable rate. The critic currently has no relevance or target-consistency coordinate capable of preventing diversity from being maximised by noise.

### 5. The critic is not a truth oracle

A structurally complete but deliberately wrong path received exactly the same energy as the gold path in every example. Separation rate was 0%.

This is not surprising from the current inputs: the critic sees path structure, provenance labels and contradiction flags, but no semantic correctness signal. The result confirms that Lyapunov energy must never be presented as truth probability.

## Required remediation before a positive claim

1. Define independence from source/evidence lineage only; edge-sequence variation must not create independent corroboration.
2. Add relevance or query-consistency gating before diversity and degeneracy can lower energy.
3. Make unresolved contradiction dominate any diversity benefit, either through a hard constraint or interaction term rather than a small additive coordinate.
4. Separate `stable reasoning dynamics` from `epistemically acceptable answer`; a zero energy state must be impossible when a path is irrelevant or contradictory.
5. Add a completeness/path-validity coordinate grounded in gold or verifier evidence for benchmark evaluation.
6. Freeze a development sample for weight and rule refinement; retain this exact validation sample and seed as an untouched rerun.

## Honest conclusion

The current critic is useful for detecting missing evidence and monitoring monotonic repair, but it fails the more important adversarial tests. It can be gamed by duplicate or irrelevant paths, and its energy ordering is wrong for contradiction.

It is therefore **not ready to serve as the acceptance controller for HypoKosh convergence** without remediation and a locked rerun of this benchmark.
