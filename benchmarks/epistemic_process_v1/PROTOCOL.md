# Epistemic Process Evaluation v1 — preregistration

Status: **PREREGISTERED / UNSCORED**. This protocol must be merged and its commit recorded before any score-bearing run. After the first score-bearing run, task semantics, event order, scoring definitions, and ablation meanings are frozen. Any change requires a new protocol version.

## Falsifiable question

Under evolving and contradictory evidence, does the GrapheneDB stack improve explicit evidence use and refutation-driven belief revision while reducing false convergence, without manufacturing apparent safety through blanket abstention?

Product/mechanism boundary:
- **GrapheneDB**: persistent epistemic state and evidence/provenance substrate.
- **HypoKosh**: competing-hypothesis runtime.
- **DWM**: challenge, reopen, and synthesis loop.

This protocol tests mechanisms. It does not establish semantic truth, general scientific reasoning, enterprise readiness, or superiority over unrelated systems.

## Required configurations

Run the same episodes and event order under:
1. **B0 bounded/stateless baseline** — fixed-answer decision/classification where applicable.
2. **G0 GrapheneDB core** — persistent evidence/provenance state without HypoKosh competition or DWM reopen/synthesis.
3. **G1 GrapheneDB + HypoKosh** — competing hypotheses enabled; DWM challenge/reopen/synthesis disabled.
4. **G2 full stack** — GrapheneDB + HypoKosh + DWM.

Optional external comparators (including Jev) may be reported only where their interface naturally fits. They are not the optimization target and must not replace the architecture ablation.

## Episode structure

Every scored episode contains a deterministic ordered stream with:
- an initially plausible hypothesis;
- correlated/duplicated support that must not count as independent corroboration;
- at least one viable competing hypothesis;
- admissible evidence that discriminates between hypotheses;
- decisive counterevidence against the currently preferred hypothesis;
- a post-refutation observation window in which reopen/revision can occur;
- a terminal evidence state that supports resolution or justified abstention.

The task manifest must identify evidence-family/dependency labels and the event(s) designated as decisive counterevidence **before** execution.

## Primary process metrics

### Evidence-use rate
Decision/revision events citing admissible evidence that bears on the selected hypothesis / all decision/revision events.

### Refutation-response rate
Episodes where decisive counterevidence is followed by an explicit downgrade, reopen, or revision of the contradicted hypothesis / episodes containing decisive counterevidence.

### Revision inertia
Number of subsequent evidence/decision steps for which a decisively contradicted hypothesis remains operative. Lower is better; zero means revision/reopen occurs at the first eligible transition.

### False-convergence rate
Episodes entering definitive resolution before preregistered evidence conditions for that resolution are satisfied / all episodes.

### Independent-evidence convergence
Report raw supporting items and distinct admissible evidence families separately. Correlated/duplicate family members cannot increase the independent-family count.

### Selective coverage
Report terminal answer accuracy jointly with coverage and abstention. Never present accuracy alone when configurations abstain at different rates.

### Receipt completeness
A receipt is complete only if it exposes: evidence references, evidence-family/dependency identity, active/competing hypotheses, contradiction/refutation event, challenge/reopen/revision transition when applicable, terminal status, and provenance sufficient to replay the episode.

## Anti-gaming and reporting rules

- Preserve every episode, failure, adapter error, timeout, abstention, and malformed receipt.
- No cherry-picking or dropping hard cases after execution.
- Report aggregate results **and** per-stress/per-episode results.
- Process metrics and terminal-answer metrics remain separate.
- A comparator adapter failure is an adapter failure, not a wrong answer and not a GrapheneDB win.
- No implementation tuning from score-bearing results may be folded into this protocol version.
- Negative or null ablation results are publishable evidence and become implementation priorities.
- If G1 does not materially improve the process behavior attributable to hypothesis competition, HypoKosh has not earned that claim.
- If G2 does not materially improve refutation response/reopen behavior over G1, DWM has not earned that claim.

## Reproducibility receipt

A score-bearing run must record:
- repository commit;
- protocol/manifest hashes;
- compiler/runtime versions;
- OS/architecture;
- configuration identifier (B0/G0/G1/G2);
- random seed(s), or an explicit statement that execution is deterministic;
- raw event stream;
- raw receipts;
- evaluator output;
- failures/timeouts.

## Freeze gate

Before the first score-bearing run:
1. merge this protocol;
2. commit the concrete task manifest and evaluator;
3. record SHA-256 hashes for protocol, manifest, evaluator, and task inputs;
4. verify a clean-checkout dry run that produces artifacts but is explicitly marked UNSCORED;
5. only then mark the protocol FROZEN and begin score-bearing evaluation.

Related: #25, #40.