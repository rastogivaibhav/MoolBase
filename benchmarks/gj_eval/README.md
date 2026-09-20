# GJ-Eval v1

GJ-Eval is the pre-registered GrapheneDB-vs-Jev benchmark for typed decisions under changing, contradictory and provenance-sensitive evidence.

Read `docs/benchmarks/GJ_EVAL_V1_SPEC.md` first.

## Current state

This directory is a benchmark scaffold. No Jev-vs-Graphene score is committed here.

v1 deliberately separates:
- a shared canonical evidence generator,
- system adapters,
- a common prediction schema,
- scoring.

The Jev integration is intentionally an external adapter contract until the official TypeSafe early-access client is wired. Do not invent a Jev SDK API in this repo.

## Generate visible development worlds

```bash
python3 benchmarks/gj_eval/generate_worldshift.py \
  --split development \
  --count 250 \
  --seed 1729 \
  --output /tmp/gj-eval-dev.jsonl
```

For the hidden test, supply a secret seed at execution time. Never commit it.

## Adapter contract

Each adapter receives one JSON object containing `world_id`, `timestep`, `visible_state`, `choices`, and `tests`.

It emits Prediction v1. Minimum fields are: `world_id`, `timestep`, `system`, `root_choice`, `act`, `selected_confidence`, and `latency_ms`.

Optional fields such as `choice_probabilities`, `ranked_hypotheses`, `epistemic_status`, `requested_test`, and `receipt` unlock additional metrics.

## Score predictions

```bash
python3 benchmarks/gj_eval/score.py \
  --worlds /tmp/gj-eval-dev.jsonl \
  --predictions /tmp/predictions.jsonl
```

The initial scorer only computes metrics supported by the common contract. It never fabricates a probability distribution.

## Rules

- Do not tune on the hidden test.
- Do not drop timeouts or malformed responses.
- Do not give Graphene richer evidence metadata than Jev-Structured.
- Do not modify Graphene storage semantics for this benchmark.
- Do not report a universal winner score.
