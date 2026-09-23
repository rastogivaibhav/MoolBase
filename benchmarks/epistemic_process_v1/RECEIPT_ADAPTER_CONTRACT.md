# Epistemic Process v1 — Receipt Adapter Contract

Status: **PREREGISTERED / UNSCORED**.

This contract sits between the frozen-candidate task stream and `evaluate_v1.py`. It exists to prevent configuration-specific adapters from quietly changing task semantics or manufacturing evidence that a runtime did not expose.

## Configurations

The same ordered episode is executed under:

- **B0** — bounded/stateless baseline. No persistent epistemic state, HypoKosh competition, or DWM challenge/reopen/synthesis.
- **G0** — GrapheneDB persistent evidence/provenance core only.
- **G1** — G0 + HypoKosh competing-hypothesis runtime.
- **G2** — G1 + DWM challenge/reopen/synthesis loop.

An adapter may translate runtime-native events into the common receipt schema. It may **not** infer a transition, hypothesis, evidence reference, dependency family, or terminal state that is absent from runtime-observable state.

## Required receipt shape

Each episode emits exactly one receipt:

```json
{
  "episode_id": "EP01",
  "configuration": "G2",
  "decisions": [
    {
      "step": 3,
      "status": "open|provisional|resolved|abstain",
      "hypothesis": "H1|null",
      "evidence_refs": ["E1"]
    }
  ],
  "terminal_status": "resolved|abstain|open",
  "terminal_hypothesis": "H1|H2|null",
  "evidence_refs": ["E1", "E2"],
  "hypotheses": [
    {"id": "H1", "status": "active|downgraded|refuted|resolved"}
  ],
  "transitions": [
    {
      "step": 5,
      "type": "downgrade|reopen|revise",
      "from": "H1",
      "to": "H2|null",
      "evidence_refs": ["E5"]
    }
  ],
  "adapter_receipt": {
    "adapter": "B0|G0|G1|G2",
    "runtime_events_observed": 0,
    "synthetic_semantic_events": 0,
    "notes": []
  }
}
```

`synthetic_semantic_events` **must remain zero**. Formatting, field renaming, stable identifier mapping, and serialization are not semantic synthesis.

## Configuration-specific observability

### B0

B0 may emit decisions and a terminal answer from the bounded baseline. If the baseline exposes no explicit competing-hypothesis state or revision transition, the adapter must leave those structures empty. The evaluator must therefore reveal the baseline's receipt incompleteness rather than the adapter inventing epistemic machinery.

### G0

G0 may expose persistent evidence references, provenance and terminal state. HypoKosh- or DWM-specific events are disabled. No `reopen` or hypothesis-competition event may be synthesized merely because later evidence contradicts an earlier decision.

### G1

G1 may additionally expose multiple active/competing hypotheses and HypoKosh-native downgrade/revision events. DWM is disabled. An adapter must not relabel generic runtime retries as DWM `reopen` events.

### G2

G2 may expose the full GrapheneDB + HypoKosh + DWM event surface. A `reopen` receipt requires a runtime-observable DWM challenge/reopen transition; a later changed answer alone is insufficient.

## Failure-preservation rules

Adapters must emit or preserve, never suppress:

- missing runtime event needed for a receipt field;
- malformed runtime event;
- unsupported configuration capability;
- timeout/crash;
- unknown evidence identifier;
- runtime event whose provenance cannot be mapped to the preregistered episode.

Adapter failure is not a wrong answer and is not a win for another configuration. It is reported separately and becomes an implementation/reproducibility priority.

## Evidence-family rule

The adapter copies preregistered evidence identity and family/dependency metadata only when the runtime event can be tied to that exact input event. It must not infer that two observations are independent because they arrived through different graph paths, agents, prompts, or messages.

## Anti-false-convergence rule

The adapter does not decide whether convergence is justified. It reports runtime state. `evaluate_v1.py` decides false convergence against the preregistered minimum independent-family condition. This separation prevents an adapter from repairing the behavior it is meant to measure.

## Revision/provenance rule

A terminal answer changing from H1 to H2 is not by itself evidence of explainable belief revision. The receipt must preserve the runtime-observable evidence references and transition that caused the change. Missing rationale remains missing.

## Dry-run gate

Before score-bearing execution:

1. implement B0/G0/G1/G2 adapters against this contract;
2. run all four configurations from a clean checkout in `unscored-dry-run` mode;
3. preserve every adapter/schema failure;
4. inspect whether any adapter is synthesizing semantic events;
5. hash protocol, task manifest, evaluator, adapter contract, adapters, and task inputs;
6. freeze those artifacts before enabling score-bearing mode.

No result from the dry run may be advertised as a benchmark score.

## External landscape rationale

The contract deliberately distinguishes *reasoning state* from execution traces and correlated reports from independent evidence. Current work on reasoning provenance argues that state/execution traces do not reconstruct why an agent changed its conclusion; recent multi-agent work shows correlated forward reasoning can share errors; and scientific-agent work increasingly maintains explicit competing hypotheses and discriminating interventions. GrapheneDB's test must therefore measure the claimed epistemic process rather than let adapters reconstruct it after the fact.

Related: #25, #40.