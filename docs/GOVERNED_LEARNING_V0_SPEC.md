# Governed HypoKosh and Outcome-Learning Specification

## Status and scope

- Specification ID: `GDB-GL-0`
- Version: `0.1`
- Product scope: experimental controlled-pilot
- Durable-format decision: no new record type and no storage-format change

This specification defines the first runnable feedback loop connecting
HypoKosh-style hypothesis proposals, bounded dialectic reasoning, verified
outcome memory, data-utility measurement, and retrieval-policy selection.

The database does not train or mutate LLM weights. It produces governed
training eligibility and policy recommendations that an external model
training system may consume. A live policy changes only after an explicit
approved decision. Stored text never changes tool authority.

## System boundary

```text
query and current policy
  -> bounded dialectic expansion
  -> read-only HypoKosh hypothesis proposals and discriminating tests
  -> externally approved action
  -> observed and independently verified outcome
  -> immutable learning episode
  -> offline train/development policy evaluation
  -> explicit promotion or rollback event
  -> next query uses the latest approved policy
```

GrapheneDB remains the authoritative durable engine. Learning episodes and
policy decisions are ordinary versioned nodes written through
`put_extraction()`. This preserves existing WAL, checkpoint, backup, replay,
validation, and storage-format behavior.

## Epistemic and control invariants

1. HypoKosh output is always a proposal with `Hypothetical` origin. Retrieval
   of observed evidence does not turn the newly generated proposal itself into
   observed truth.
2. Hypothesis generation and policy evaluation are read-only.
3. Only human-verified training-split episodes are eligible for learning.
   Development episodes may select between candidates. Evaluation episodes
   are measured and reported but never used to fit or select a policy.
4. False promotion, harmful action, unauthorized action, or harmful-memory
   activation makes an episode safety-negative rather than silently excluding
   it.
5. Policy evaluation may recommend but cannot activate a policy.
6. Promotion and rollback require an explicit approval flag, approver identity,
   evaluation reference, and idempotent event identity.
7. Policy history is append-only. Rollback creates a new decision event; it
   does not erase the policy being rolled back.
8. An episode under legal hold cannot be quarantined.
9. Quarantined episodes must stop influencing subsequent policy evaluation.
10. Tenant IDs scope episode and policy identities. This v0 API uses one pilot
    credential and does not claim cryptographic tenant isolation; production
    use requires an identity-aware gateway that binds the authenticated
    principal to the tenant.

## Learning episode

An episode contains:

- tenant, episode, family, domain, and dataset-split identity;
- query text, vector, semantic signature, model version, and policy version;
- the bounded retrieval-policy parameters used;
- verified outcome identity and verifier;
- task success, causal F1, evidence coverage, and calibration error;
- latency, token cost, and action cost;
- false-promotion, harmful-action, unauthorized-action, expired-truth, and
  harmful-memory signals;
- intermediate and retained trace bytes;
- decisive-evidence retention and useful-evidence recall-at-20 counts;
- evidence node IDs and legal-hold state.

The deterministic engineering utility is:

```text
utility =
    0.35 * task_success
  + 0.20 * causal_f1
  + 0.15 * evidence_coverage
  + 0.10 * decisive_evidence_retention
  + 0.10 * useful_evidence_recall_at_20
  + 0.10 * (1 - calibration_error)
  - bounded_latency_token_and_action_cost
```

Any false promotion, harmful action, unauthorized action, or harmful-memory
activation forces utility to `-1`. This score is an engineering ranking
signal, not a calibrated probability.

## Policy learning

A retrieval policy versions:

- semantic candidate count;
- maximum hops;
- path and per-root path budgets;
- visited-state budget;
- opposition-round budget;
- convergence threshold;
- re-expansion threshold.

The evaluator groups verified episodes by exact policy configuration. It
computes training and development utility, development task success,
per-domain regressions, safety violations, trace-retention ratio,
decisive-evidence retention, useful-evidence recall-at-20, harmful-memory
activation, and expired-truth activation.

A candidate is recommendable only when:

- it meets minimum training and development sample counts;
- its development utility exceeds the named baseline by the configured
  margin;
- it has zero safety violations;
- no jointly represented development domain regresses beyond the configured
  limit.

Evaluation-split outcomes are counted as excluded evidence and cannot affect
the recommendation.

## Data-use decisions

Every episode receives one of:

- `decisive`
- `useful`
- `redundant`
- `stale`
- `misleading`
- `harmful`
- `never_eligible`

The evaluator recommends archiving intermediate trace bytes for redundant,
stale, misleading, harmful, and never-eligible episodes. It never recommends
archiving decisive evidence merely because it is large.

These are recommendations. Physical archival and compliance retention remain
external policy operations.

## API surface

All routes except existing public health/version routes require the pilot API
credential and the existing HTTPS-proxy policy:

```text
POST /v1/reason/hypokosh
POST /v1/learning/episodes
POST /v1/learning/policies/evaluate
POST /v1/learning/policies/decisions
GET  /v1/learning/policies/current?tenant_id=...
POST /v1/learning/episodes/quarantine
```

The reasoning route is read-only. Every write route is bounded by the global
request limit and the existing bounded worker queue. Episode and decision
identity is durable and conflict-safe across restart.

## Requirements and verification trace

| ID | Requirement | Required evidence |
|---|---|---|
| GL-001 | HypoKosh produces bounded proposals and discriminating tests from a snapshot-pinned dialectic result. | `tests/test_governed_learning.cpp` |
| GL-002 | Every generated proposal remains `Hypothetical` and cannot claim observed/discovered truth. | `tests/test_governed_learning.cpp` |
| GL-003 | HypoKosh reasoning performs zero durable writes. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-004 | Learning episodes validate dimensions, bounds, counts, finite metrics, identity, and verification fields. | `tests/test_governed_learning.cpp` |
| GL-005 | One episode is committed through one extraction transaction with durable conflict-safe replay. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-006 | Changed replay is rejected and identical replay writes nothing before and after restart. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-007 | Only verified training episodes are training eligible. | `tests/test_governed_learning.cpp` |
| GL-008 | Evaluation outcomes are excluded from fitting and policy selection. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-009 | Safety-negative episodes receive utility `-1` and remain visible in evaluation. | `tests/test_governed_learning.cpp` |
| GL-010 | Utility and data-use classification are deterministic and bounded. | `tests/test_governed_learning.cpp` |
| GL-011 | Policy comparison enforces sample, improvement, safety, and worst-domain-regression gates. | `tests/test_governed_learning.cpp` |
| GL-012 | Policy evaluation is read-only and cannot activate a candidate. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-013 | Policy activation requires approval, approver, evaluation reference, and event identity. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-014 | Promotion history is append-only and rollback restores a prior exact policy through a new event. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-015 | Current-policy lookup survives checkpoint and restart. | `tests/test_governed_learning.cpp`, `scripts/server_learning_contract_test.py` |
| GL-016 | Legal hold blocks quarantine; ordinary quarantine removes future policy influence. | `tests/test_governed_learning.cpp` |
| GL-017 | Training-example export excludes unverified, development, evaluation, and safety-negative episodes. | `tests/test_governed_learning.cpp` |
| GL-018 | Data-policy metrics separately report trace retention, decisive retention, useful recall, harmful activation, and expired-truth activation. | `tests/test_governed_learning.cpp` |
| GL-019 | The HTTP contract is authenticated, bounded, strict JSON, and represented in OpenAPI and the Python client. | `scripts/server_learning_contract_test.py`, `tests/test_openapi_surface.py` |
| GL-020 | Existing durable formats remain unchanged and existing release contracts continue to pass. | full CTest, `docs/STORAGE_FORMAT.md` |
| GL-021 | The production Docker image runs the loop as the non-root user with a read-only root filesystem. | Docker learning-loop validation |
| GL-022 | The implementation makes no self-training, calibrated-probability, tenant-isolation, distributed, or autonomous-action claim. | `docs/GOVERNED_LEARNING_V0_SPEC.md` and release documentation |

## Acceptance for GDB-GL-0

This implementation is accepted only when:

- the specification validator and all requirement-linked tests pass;
- the full release CTest suite passes;
- package-consumer verification passes;
- an end-to-end Docker run records episodes, recommends a better policy,
  refuses unapproved promotion, activates an approved policy, uses it for a
  HypoKosh request, rolls it back, and survives restart;
- `git diff --check` reports no error.

Passing `GDB-GL-0` demonstrates a governed learning mechanism. It does not pass
the longitudinal `G8` or data-efficiency `G9` research gates in
`deepmindtest.md`; those still require the preregistered blinded datasets,
1,000 verified training episodes, 300 held-out episodes, confidence intervals,
and external reproduction.
