# Independent MoolBase reproduction

This is the shortest external path for testing the current MoolBase flagship mechanism.

You do **not** need to read the historical GrapheneDB, HypoKosh or DWM papers first.

## What this exercises

The current flagship proof checks a bounded set of mechanisms:

- correlated paths must not fabricate independent corroboration;
- competing hypotheses remain visible without being mislabeled as material opposition;
- material contradiction can block final resolution;
- a dialectical challenge can request bounded reopen/re-expansion;
- missing-hop recovery expands the declared depth frontier rather than silently widening another search dimension;
- discriminating evidence can change the governed evidence state;
- the mechanism receipt remains deterministic for the frozen scenario.

The public product name is **MoolBase**. Historical implementation identifiers such as GrapheneDB, HypoKosh and DWM remain visible in APIs, binaries and research artifacts for compatibility.

## Prerequisites

- Git
- Python 3
- CMake 3.16 or newer
- a C++20 compiler

No model API key, hosted LLM or external service is required after cloning the repository.

## One-command reproduction

~~~bash
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
python3 scripts/run_flagship_proof.py
~~~

The runner builds the canonical demo, executes it, validates the current contract and writes a deterministic receipt.

## Expected canonical mechanism receipt

~~~text
12f2c843774027b33b2e81869fc24b232f849e81f84936d7d7b8b1be189fde89
~~~

The mechanism hash excludes the current commit identity. A separate provenance hash binds the observed mechanism result to the exact source commit used for the run.

## Output

The command writes:

~~~text
reports/flagship-proof/
  receipt.json
  scenario_manifest.json
  raw_output.txt
  summary.md
~~~

A mismatch is useful evidence. The runner fails closed if the current mechanism receipt differs from the canonical receipt.

## How to try to break it

The most useful external work is not another happy-path replay. Try to falsify the mechanism.

Useful perturbations include:

1. duplicate a support path while keeping the same evidence family — independent corroboration must not increase;
2. remove one genuinely independent support family — corroboration must fall rather than remain inflated;
3. inject material contradiction — final resolution must remain blocked while the contradiction is operative;
4. reorder deterministic evidence insertion or path presentation — the epistemic result should remain semantically stable;
5. reduce the permitted search/depth budget — the receipt must expose the changed frontier/stop decision rather than silently widening another dimension.

A bug is especially valuable if one of these causes silent promotion, contradiction loss, fabricated independence, unsupported re-expansion or an unexplained mechanism-receipt change.

## Historical perturbation packs

The repository contains frozen historical perturbation artifacts, including V1, plus a V3-aligned V2 preregistration.

Do **not** use the historical V1 perturbation runner as the newcomer reproduction command. It intentionally preserves its earlier frozen canonical contract and can reject the current V3-aligned flagship semantics.

Historical failures and older receipts are retained as evidence rather than rewritten.

## Report an independent reproduction

Use the **Independent reproduction report** issue template and include:

- operating system;
- compiler and version;
- Python version;
- CMake version;
- exact commit or release;
- proof / benchmark / protocol name;
- mechanism receipt hash;
- whether the instructions worked without assistance;
- anything confusing or incorrect;
- any adversarial case that should be added.

Failed reproductions, null results and criticism are explicitly welcome.

## Claim boundary

Passing this proof does not establish semantic truth, autonomous scientific discovery, automatic hidden-dependence discovery, durable cross-run DWM belief promotion, general superiority over other systems, or enterprise readiness.

It establishes only the bounded mechanisms encoded by the current flagship scenario and runner.
