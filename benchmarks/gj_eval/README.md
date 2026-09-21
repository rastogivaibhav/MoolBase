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

The Jev integration is wired to TypeSafe's documented HTTP endpoint (`https://api.typesafe.ai/v1/systemone`) through `adapters/jev_http.py`. The live API key remains external and must never be committed.

## Generate visible development worlds

```bash
python3 benchmarks/gj_eval/generate_worldshift.py \
  --split development \
  --count 240 \
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

## Harness self-test

This makes no provider/model calls:

```bash
python3 benchmarks/gj_eval/self_test.py
```

It verifies deterministic generation, hidden-oracle redaction, valid command-adapter plumbing, and that malformed adapter output remains a scored failure.

## Run any external adapter

```bash
python3 benchmarks/gj_eval/run_command_adapter.py \
  --worlds /tmp/gj-eval-dev.jsonl \
  --adapter-cmd "python3 /path/to/adapter.py" \
  --system jev_structured \
  --state-mode structured \
  --output /tmp/jev-structured.jsonl
```

For Jev-Raw use `--state-mode raw`. For Jev-Structured use `--state-mode structured`.
The official TypeSafe integration should live behind the adapter command so the benchmark does not guess a private/unstable SDK contract.


## Official Jev arms

Set the live credential outside the repository:

```bash
export TYPESAFE_API_KEY="..."
```

J1 Structured:

```bash
python3 benchmarks/gj_eval/run_command_adapter.py \
  --worlds /tmp/gj-eval-dev.jsonl \
  --adapter-cmd "python3 benchmarks/gj_eval/adapters/jev_http.py" \
  --system jev_structured \
  --state-mode structured \
  --output /tmp/jev-structured.jsonl
```

J0 Raw uses the same adapter with `--state-mode raw` and system `jev_raw`.

The Jev adapter asks a root-cause Choice, an evidence-sufficiency Noul, and (when tests are available) a next-test Choice. Calibration uses the selected root probability; Jev's native Choice confidence is preserved separately in the receipt.

## Graphene ablations

See `ablation_status.v1.json`. G0, G1, G3 and G6 have executable adapters. G2, G4 and G5 are explicitly `NOT_SCORED` until the runtime exposes clean isolation points.

## Paired comparison

```bash
python3 benchmarks/gj_eval/compare.py \
  --worlds /tmp/gj-eval-dev.jsonl \
  --predictions /tmp/all-primary-predictions.jsonl \
  --system-a graphenedb_full \
  --system-b jev_structured
```

Bootstrap resampling is clustered by `scenario_id`, so variants from one latent world are not counted as independent samples.

## Reproducibility receipt

```bash
python3 benchmarks/gj_eval/freeze_protocol.py --output /tmp/gj-eval-freeze.json
```

When the hidden test is eventually executed, `GJ_EVAL_TEST_SEED` is supplied only at runtime; the receipt stores its SHA-256, never the seed itself.
